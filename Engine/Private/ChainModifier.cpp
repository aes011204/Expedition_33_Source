#include "ChainModifier.h"
#include "ModelCom.h"
#include "Bone.h"
#include "GameObject.h"
#include "Helper_Math.h"
#include "ModelPhysicsParser.h"
#include "Reflection_Types.h"
#include "TransformCom.h"
#include "GameInstance.h"
#include "Engine_Macro.h"
#include <algorithm>
#include <cmath>

NS_BEGIN(Engine)

namespace
{
	template<typename Member>
	size_t ChainShapeOffset(Member ChainModifier::ChainModifierDesc::ShapeSettings::* member)
	{
		ChainModifier::ChainModifierDesc desc{};
		const auto *base = reinterpret_cast<const char *>(&desc);
		const auto *field = reinterpret_cast<const char *>(&(desc.shape.*member));
		return static_cast<size_t>(field - base);
	}

	template<typename Member>
	size_t ChainMotionOffset(Member ChainModifier::ChainModifierDesc::MotionSettings::* member)
	{
		ChainModifier::ChainModifierDesc desc{};
		const auto *base = reinterpret_cast<const char *>(&desc);
		const auto *field = reinterpret_cast<const char *>(&(desc.motion.*member));
		return static_cast<size_t>(field - base);
	}

	template<typename Member>
	size_t ChainCollisionOffset(Member ChainModifier::ChainModifierDesc::CollisionSettings::* member)
	{
		ChainModifier::ChainModifierDesc desc{};
		const auto *base = reinterpret_cast<const char *>(&desc);
		const auto *field = reinterpret_cast<const char *>(&(desc.collision.*member));
		return static_cast<size_t>(field - base);
	}

	template<typename Member>
	size_t ChainRuntimeOffset(Member ChainModifier::ChainModifierDesc::RuntimeSettings::* member)
	{
		ChainModifier::ChainModifierDesc desc{};
		const auto *base = reinterpret_cast<const char *>(&desc);
		const auto *field = reinterpret_cast<const char *>(&(desc.runtime.*member));
		return static_cast<size_t>(field - base);
	}

	void AddRuntimeProperty(
		vector<PropertyInfo> &properties,
		PropertyType type,
		const string &name,
		const string &displayName,
		size_t offset,
		float dragSpeed = 0.01f,
		float minVal = 0.f,
		float maxVal = 0.f)
	{
		PropertyInfo property{};
		property.name = name;
		property.displayName = displayName;
		property.type = type;
		property.offset = offset;
		property.dragSpeed = dragSpeed;
		property.minVal = minVal;
		property.maxVal = maxVal;
		properties.push_back(std::move(property));
	}

	Matrix BuildTranslationMatrix(const Vec3 &translation)
	{
		return Matrix::CreateTranslation(translation);
	}

	Quaternion BuildRootOffsetQuaternion(const Vec3 &rotationOffsetDegrees)
	{
		return Quaternion::CreateFromYawPitchRoll(
			XMConvertToRadians(rotationOffsetDegrees.y),
			XMConvertToRadians(rotationOffsetDegrees.x),
			XMConvertToRadians(rotationOffsetDegrees.z));
	}

	float GetChainTipRatio(const ChainModifier::BoneChain &chain, size_t nodeIndex);

	float GetWindNodeWeight(const ChainModifier::BoneChain &chain, size_t nodeIndex)
	{
		if (chain.nodes.size() <= 1)
		{
			return 0.f;
		}

		const float ratio = GetChainTipRatio(chain, nodeIndex);
		const float startRatio = 0.4f;
		const float tipRatio = Math::NormalizeSegment(ratio, startRatio, 1.f);
		const float weightScale = 0.65f;
		return Math::GetSmoothStepRatio(tipRatio) * weightScale;
	}

	uint32 GetNodeDepthFromRoot(const ChainModifier::BoneChain &chain, size_t nodeIndex, size_t rootIndex)
	{
		uint32 depth = 0;
		size_t currentIndex = nodeIndex;
		while (currentIndex != rootIndex && currentIndex < chain.nodes.size())
		{
			const int32 parentIndex = chain.nodes[currentIndex].parentNodeIndex;
			if (parentIndex < 0)
				break;

			currentIndex = static_cast<size_t>(parentIndex);
			++depth;
		}

		return depth;
	}

	size_t GetPhysicalRootIndex(const ChainModifier::BoneChain &chain, size_t nodeIndex)
	{
		size_t rootIndex = nodeIndex;
		while (rootIndex < chain.nodes.size())
		{
			const int32 parentIndex = chain.nodes[rootIndex].parentNodeIndex;
			if (parentIndex < 0)
				break;

			const auto &parentNode = chain.nodes[static_cast<size_t>(parentIndex)];
			if (parentNode.isVirtualAnchor)
				break;

			rootIndex = static_cast<size_t>(parentIndex);
		}

		return rootIndex;
	}

	size_t GetBranchRootIndex(const ChainModifier::BoneChain &chain, size_t nodeIndex, size_t rootIndex)
	{
		size_t branchRootIndex = nodeIndex;
		while (branchRootIndex < chain.nodes.size())
		{
			const int32 parentIndex = chain.nodes[branchRootIndex].parentNodeIndex;
			if (parentIndex < 0 || static_cast<size_t>(parentIndex) == rootIndex)
				break;

			branchRootIndex = static_cast<size_t>(parentIndex);
		}

		return branchRootIndex;
	}

	uint32 GetMaxDescendantDepth(const ChainModifier::BoneChain &chain, size_t nodeIndex)
	{
		if (nodeIndex >= chain.nodes.size() || chain.nodes[nodeIndex].childNodeIndices.empty())
			return 0;

		uint32 maxDepth = 0;
		for (const int32 childIndex : chain.nodes[nodeIndex].childNodeIndices)
		{
			if (childIndex < 0)
				continue;

			maxDepth = std::max(maxDepth, 1u + GetMaxDescendantDepth(chain, static_cast<size_t>(childIndex)));
		}

		return maxDepth;
	}

	float CalculateChainTipRatio(const ChainModifier::BoneChain &chain, size_t nodeIndex)
	{
		if (chain.nodes.size() <= 1 || nodeIndex >= chain.nodes.size())
			return 0.f;

		const size_t rootIndex = GetPhysicalRootIndex(chain, nodeIndex);
		const uint32 nodeDepth = GetNodeDepthFromRoot(chain, nodeIndex, rootIndex);
		if (nodeDepth == 0)
			return 0.f;

		const size_t branchRootIndex = GetBranchRootIndex(chain, nodeIndex, rootIndex);
		const uint32 branchDepth = GetNodeDepthFromRoot(chain, branchRootIndex, rootIndex);
		const uint32 maxDepth = std::max(branchDepth + GetMaxDescendantDepth(chain, branchRootIndex), 1u);
		const float ratio = std::clamp(static_cast<float>(nodeDepth) / static_cast<float>(maxDepth), 0.f, 1.f);
		return Math::GetSmoothStepRatio(ratio);
	}

	float GetChainTipRatio(const ChainModifier::BoneChain &chain, size_t nodeIndex)
	{
		if (nodeIndex >= chain.nodes.size())
			return 0.f;

		return chain.nodes[nodeIndex].tipRatio;
	}

	void CacheChainTipRatios(ChainModifier::BoneChain &chain)
	{
		for (size_t nodeIndex = 0; nodeIndex < chain.nodes.size(); ++nodeIndex)
		{
			chain.nodes[nodeIndex].tipRatio = CalculateChainTipRatio(chain, nodeIndex);
		}
	}

	float BlendTipScale(float rootValue, float tipScale, float tipRatio)
	{
		return rootValue * (1.f + (tipScale - 1.f) * tipRatio);
	}

	Vec3 ComputeWindOffset(
		const ChainModifier::ChainModifierDesc &desc,
		const ChainModifier::BoneChain &chain,
		const ChainModifier::BoneChainNode &node,
		size_t nodeIndex)
	{
		if (desc.runtime.windScale == 0.f || desc.runtime.windDirection.LengthSquared() <= 0.0001f)
		{
			return Vec3::Zero;
		}

		Vec3 windDir = desc.runtime.windDirection;
		windDir.Normalize();

		float nodeWeight = GetWindNodeWeight(chain, nodeIndex);
		if (desc.runtime.windStartRatio >= 0.f || desc.runtime.windWeightScale >= 0.f)
		{
			const float ratio = GetChainTipRatio(chain, nodeIndex);
			const float startRatio = desc.runtime.windStartRatio >= 0.f
				? desc.runtime.windStartRatio
				: 0.4f;
			const float weightScale = desc.runtime.windWeightScale >= 0.f
				? desc.runtime.windWeightScale
				: 0.65f;
			const float tipRatio = Math::NormalizeSegment(ratio, startRatio, 1.f);
			nodeWeight = Math::GetSmoothStepRatio(tipRatio) * weightScale;
		}
		if (nodeWeight <= 0.f)
		{
			return Vec3::Zero;
		}

		Vec3 lateralWind = windDir;
		float lateralAmount = 1.f;

		if (node.parentNodeIndex >= 0)
		{
			const auto &parentNode = chain.nodes[node.parentNodeIndex];
			Vec3 chainDir = node.originalWorld.Translation() - parentNode.originalWorld.Translation();
			if (chainDir.LengthSquared() > 0.0001f)
			{
				chainDir.Normalize();
				const float parallel = windDir.Dot(chainDir);
				lateralWind = windDir - chainDir * parallel;
				lateralAmount = 1.f - std::abs(parallel);
			}
		}

		if (lateralWind.LengthSquared() <= 0.0001f)
		{
			return Vec3::Zero;
		}

		lateralWind.Normalize();
		lateralAmount = Math::GetSmoothStepRatio(std::clamp(lateralAmount, 0.f, 1.f));
		return lateralWind * (desc.runtime.windScale * chain.windMul * node.length * nodeWeight * lateralAmount);
	}
}

void ChainModifier::PurseModifier::Prepare(const BoneChain &chain)
{
	_isPrepared = false;
	if (chain.nodes.empty())
	{
		return;
	}

	const auto &anchorNode = chain.nodes.front();
	_currentAnchorWorld = anchorNode.originalWorld;
	_previousAnchorWorld = anchorNode.prevOriginalWorld;
	_currentAnchorInv = _currentAnchorWorld.Invert();
	_isPrepared = true;
}

Vec3 ChainModifier::PurseModifier::Compute_RootMotionOffset(const BoneChain &chain, size_t nodeIndex) const
{
	if (!_isPrepared || nodeIndex >= chain.nodes.size())
	{
		return Vec3::Zero;
	}

	const auto &node = chain.nodes[nodeIndex];
	const Vec3 nodeAnchorLocal = Vec3::Transform(node.originalWorld.Translation(), _currentAnchorInv);
	const Vec3 previousDrivenWorld = Vec3::Transform(nodeAnchorLocal, _previousAnchorWorld);
	const Vec3 currentDrivenWorld = Vec3::Transform(nodeAnchorLocal, _currentAnchorWorld);

	return previousDrivenWorld - currentDrivenWorld;
}

HRESULT ChainModifier::Initialize(AnimationModifierDesc *desc)
{
	if (FAILED(__super::Initialize(desc)))
	{
		return E_FAIL;
	}

	auto chainDesc = static_cast<ChainModifierDesc *>(desc);
	if (!chainDesc)
	{
		return E_FAIL;
	}

	_desc = *chainDesc;

	if (!_desc.collision.overrideCapsules.empty())
	{
		_colliders = _desc.collision.overrideCapsules;
	}
	else if (!_desc.collision.physicsAssetPath.empty())
	{
		_colliders = ModelPhysicsParser::LoadCapsulesFromJSON(_desc.collision.physicsAssetPath);
	}

	return S_OK;
}

void ChainModifier::BuildChainRecursive(ModelCom *model, int32 currentBoneIndex, int32 parentNodeIndex, BoneChain &outChain, uint32 currentDepth, uint32 maxDepth)
{
	BoneChainNode node;
	node.boneIndex = currentBoneIndex;
	node.parentNodeIndex = parentNodeIndex;
	node.bindLocal = model->Get_BoneLocalMatrix(currentBoneIndex);

	if (parentNodeIndex != -1)
	{
		Vec3 localTranslation = node.bindLocal.Translation();
		node.length = localTranslation.Length();
		if (node.length < 0.01f)
		{
			node.length = 0.01f;
		}
	}

	int32 thisNodeIndex = static_cast<int32>(outChain.nodes.size());
	outChain.nodes.push_back(node);

	if (parentNodeIndex != -1)
	{
		outChain.nodes[parentNodeIndex].childNodeIndices.push_back(thisNodeIndex);
	}

	bool addedChild = false;
	if (currentDepth < maxDepth)
	{
		const uint32 numBones = static_cast<uint32>(model->Get_NumBones());
		for (uint32 i = 0; i < numBones; ++i)
		{
			if (model->Get_BoneParentIndex(i) == currentBoneIndex)
			{
				addedChild = true;
				BuildChainRecursive(model, static_cast<int32>(i), thisNodeIndex, outChain, currentDepth + 1, maxDepth);
			}
		}
	}

	if (!addedChild && outChain.dummyEndLength > 0.f)
	{
		BoneChainNode tipNode;
		tipNode.boneIndex = currentBoneIndex;
		tipNode.parentNodeIndex = thisNodeIndex;
		tipNode.length = outChain.dummyEndLength;
		tipNode.isVirtualTip = true;

		const int32 tipNodeIndex = static_cast<int32>(outChain.nodes.size());
		outChain.nodes.push_back(tipNode);
		outChain.nodes[thisNodeIndex].childNodeIndices.push_back(tipNodeIndex);
	}
}

void ChainModifier::BuildBindLocalDirections(ModelCom *model, BoneChain &chain, const Matrix &ownerWorld)
{
	vector<Matrix> bindWorlds(chain.nodes.size(), Matrix::Identity);

	for (size_t i = 0; i < chain.nodes.size(); ++i)
	{
		auto &node = chain.nodes[i];

		if (node.isVirtualAnchor)
		{
			const Matrix *combinedMat = model->Get_BoneMatrixPtr(node.boneIndex);
			bindWorlds[i] = combinedMat ? (*combinedMat) * ownerWorld : ownerWorld;
			node.bindLocalDir = Vec3::Zero;
			node.poseLocal = node.bindLocal;
			node.poseLocalDir = Vec3::Zero;
			continue;
		}

		if (node.isVirtualTip)
		{
			const Matrix &parentBindWorld = bindWorlds[node.parentNodeIndex];
			Vec3 localTipDir(0.f, -1.f, 0.f);
			localTipDir.Normalize();
			node.bindLocalDir = localTipDir;
			node.poseLocal = Matrix::Identity;
			node.poseLocalDir = localTipDir;

			Vec3 worldTipDir = Get_WorldDirection(parentBindWorld, localTipDir);
			if (worldTipDir.LengthSquared() <= 0.0001f)
			{
				worldTipDir = Vec3::UnitY;
			}

			worldTipDir.Normalize();
			const Vec3 tipPosition = parentBindWorld.Translation() + worldTipDir * node.length;
			bindWorlds[i] = BuildTranslationMatrix(tipPosition);
			continue;
		}

		if (node.parentNodeIndex < 0)
		{
			bindWorlds[i] = node.bindLocal * ownerWorld;
			node.bindLocalDir = Vec3::Zero;
			node.poseLocal = node.bindLocal;
			node.poseLocalDir = Vec3::Zero;
			continue;
		}

		const Matrix &parentBindWorld = bindWorlds[node.parentNodeIndex];
		Matrix bindLocal = node.bindLocal;
		const bool applyRootOffset = chain.nodes[node.parentNodeIndex].isVirtualAnchor;
		const Vec3 rootOffsetDegrees = chain.rootRotationOffsetDegrees;
		if (applyRootOffset
			&& rootOffsetDegrees.LengthSquared() > 0.0001f)
		{
			bindLocal = Build_LocalMatrix_RotationOnly(node.bindLocal, BuildRootOffsetQuaternion(rootOffsetDegrees));
		}

		bindWorlds[i] = bindLocal * parentBindWorld;

		Vec3 bindWorldDir = bindWorlds[i].Translation() - parentBindWorld.Translation();
		if (bindWorldDir.LengthSquared() <= 0.0001f)
		{
			node.bindLocalDir = Vec3::Zero;
			node.poseLocal = bindLocal;
			node.poseLocalDir = Vec3::Zero;
			continue;
		}

		node.bindLocalDir = Get_LocalDirection(parentBindWorld, bindWorldDir);
		node.poseLocal = bindLocal;
		node.poseLocalDir = node.bindLocalDir;
	}
}

Matrix ChainModifier::Resolve_OwnerWorldMatrix() const
{
	if (_desc.runtime.ownerWorldMatrix)
	{
		return *_desc.runtime.ownerWorldMatrix;
	}

	if (_desc.runtime.ownerTransform)
	{
		return _desc.runtime.ownerTransform->Get_WorldMatrix();
	}

	return Matrix::Identity;
}

void ChainModifier::Initialize_Chains(ModelCom *model)
{
	_chains.clear();
	Matrix ownerWorld = Resolve_OwnerWorldMatrix();

	for (const auto &groupDesc : _desc.groups)
	{
		int32 rootBoneIndex = model->Get_BoneIndex(groupDesc.rootBoneName);
		if (rootBoneIndex == -1)
		{
			continue;
		}

		BoneChain newChain;
		newChain.rootBoneName = groupDesc.rootBoneName;
		newChain.stiffnessMul = groupDesc.stiffnessMul;
		newChain.dampingMul = groupDesc.dampingMul;
		newChain.gravityMul = groupDesc.gravityMul;
		newChain.movementInertiaMul = groupDesc.movementInertiaMul;
		newChain.poseRestoreMul = groupDesc.poseRestoreMul;
		newChain.maxBendAngleMul = groupDesc.maxBendAngleMul;
		newChain.windMul = groupDesc.windMul;
		newChain.dummyEndLength = groupDesc.dummyEndLength;
		newChain.rootRotationOffsetDegrees = groupDesc.rootRotationOffsetDegrees;

		int32 realParentBoneIndex = model->Get_BoneParentIndex(rootBoneIndex);
		if (realParentBoneIndex != -1)
		{
			BoneChainNode anchorNode;
			anchorNode.boneIndex = realParentBoneIndex;
			anchorNode.parentNodeIndex = -1;
			anchorNode.isVirtualAnchor = true;

			Matrix localMat = model->Get_BoneLocalMatrix(rootBoneIndex);
			anchorNode.length = localMat.Translation().Length();
			if (anchorNode.length < 0.01f)
			{
				anchorNode.length = 0.01f;
			}

			newChain.nodes.push_back(anchorNode);
			BuildChainRecursive(model, rootBoneIndex, 0, newChain, 0, groupDesc.maxDepth);
		}
		else
		{
			BuildChainRecursive(model, rootBoneIndex, -1, newChain, 0, groupDesc.maxDepth);
		}

		if (!newChain.nodes.empty())
		{
			BuildBindLocalDirections(model, newChain, ownerWorld);
			CacheChainTipRatios(newChain);

			_chains.push_back(std::move(newChain));
		}
	}

	_chainsInitialized = true;
}

void ChainModifier::ResetPose(ModelCom *model)
{
	Matrix ownerWorld = Resolve_OwnerWorldMatrix();

	Capture_CurrentPose(model, ownerWorld);

	for (auto &chain : _chains)
	{
		for (auto &node : chain.nodes)
		{
			node.prevOriginalWorld = node.originalWorld;
			node.prevOriginalPosition = node.originalWorld.Translation();
			node.prevPosition = node.originalWorld.Translation();
			node.position = node.prevPosition;
		}
	}
}

const ClassReflectionInfo *ChainModifier::Get_RuntimeReflectionInfo() const
{
	static ClassReflectionInfo info = []()
		{
			ClassReflectionInfo result{};
			result.className = "ChainModifierRuntime";
			result.displayName = "Chain Modifier Runtime";
			result.category = "Animation";
			result.type = RegisteredType::None;

			AddRuntimeProperty(result.properties, PropertyType::Float, "shape.stiffness", "Stiffness", ChainShapeOffset(&ChainModifierDesc::ShapeSettings::stiffness), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "shape.tipStiffnessScale", "Tip Stiff", ChainShapeOffset(&ChainModifierDesc::ShapeSettings::tipStiffnessScale), 0.01f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "shape.poseRestoreStrength", "Restore", ChainShapeOffset(&ChainModifierDesc::ShapeSettings::poseRestoreStrength), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "shape.tipPoseRestoreScale", "Tip Restore", ChainShapeOffset(&ChainModifierDesc::ShapeSettings::tipPoseRestoreScale), 0.01f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "shape.maxBendAngleDegree", "Max Bend", ChainShapeOffset(&ChainModifierDesc::ShapeSettings::maxBendAngleDegree), 0.25f, 0.f, 180.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "shape.tipMaxBendAngleScale", "Tip Bend", ChainShapeOffset(&ChainModifierDesc::ShapeSettings::tipMaxBendAngleScale), 0.01f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "shape.surfaceConstraintStrength", "Surface", ChainShapeOffset(&ChainModifierDesc::ShapeSettings::surfaceConstraintStrength), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "shape.rootOutputAlpha", "Root Out", ChainShapeOffset(&ChainModifierDesc::ShapeSettings::rootOutputAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "shape.tipOutputAlpha", "Tip Out", ChainShapeOffset(&ChainModifierDesc::ShapeSettings::tipOutputAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Bool, "shape.pinRoot", "Pin Root", ChainShapeOffset(&ChainModifierDesc::ShapeSettings::pinRoot));
			AddRuntimeProperty(result.properties, PropertyType::Float, "motion.damping", "Damping", ChainMotionOffset(&ChainModifierDesc::MotionSettings::damping), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "motion.tipDampingScale", "Tip Damp", ChainMotionOffset(&ChainModifierDesc::MotionSettings::tipDampingScale), 0.01f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "motion.movementInertiaScale", "Inertia", ChainMotionOffset(&ChainModifierDesc::MotionSettings::movementInertiaScale), 0.005f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "motion.ownerMovementInertiaScale", "Owner Inertia", ChainMotionOffset(&ChainModifierDesc::MotionSettings::ownerMovementInertiaScale), 0.005f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "motion.tipMovementInertiaScale", "Tip Inertia", ChainMotionOffset(&ChainModifierDesc::MotionSettings::tipMovementInertiaScale), 0.01f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "motion.maxMovementOffset", "Move Limit", ChainMotionOffset(&ChainModifierDesc::MotionSettings::maxMovementOffset), 0.001f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Bool, "motion.usePurseModifier", "Purse", ChainMotionOffset(&ChainModifierDesc::MotionSettings::usePurseModifier));

			AddRuntimeProperty(result.properties, PropertyType::Bool, "collision.enableCollision", "Collision", ChainCollisionOffset(&ChainModifierDesc::CollisionSettings::enableCollision));
			AddRuntimeProperty(result.properties, PropertyType::Float, "collision.particleCollisionRadius", "Col Radius", ChainCollisionOffset(&ChainModifierDesc::CollisionSettings::particleCollisionRadius), 0.01f, 0.f, 100.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "collision.collisionRadiusPadding", "Col Pad", ChainCollisionOffset(&ChainModifierDesc::CollisionSettings::collisionRadiusPadding), 0.01f, 0.f, 100.f);
			AddRuntimeProperty(result.properties, PropertyType::Bool, "collision.debugDrawCapsules", "Debug Col", ChainCollisionOffset(&ChainModifierDesc::CollisionSettings::debugDrawCapsules));

			AddRuntimeProperty(result.properties, PropertyType::Float, "runtime.resetDistance", "Reset Dist", ChainRuntimeOffset(&ChainModifierDesc::RuntimeSettings::resetDistance), 0.05f, 0.f, 100.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "runtime.targetFrameRate", "Target FPS", ChainRuntimeOffset(&ChainModifierDesc::RuntimeSettings::targetFrameRate), 1.f, 0.f, 240.f);
			AddRuntimeProperty(result.properties, PropertyType::Vec3, "runtime.gravity", "Gravity", ChainRuntimeOffset(&ChainModifierDesc::RuntimeSettings::gravity), 0.005f);
			AddRuntimeProperty(result.properties, PropertyType::Vec3, "runtime.windDirection", "Wind Dir", ChainRuntimeOffset(&ChainModifierDesc::RuntimeSettings::windDirection), 0.01f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "runtime.windScale", "Wind", ChainRuntimeOffset(&ChainModifierDesc::RuntimeSettings::windScale), 0.005f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "runtime.windStartRatio", "Wind Start", ChainRuntimeOffset(&ChainModifierDesc::RuntimeSettings::windStartRatio), 0.01f, -1.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "runtime.windWeightScale", "Wind Weight", ChainRuntimeOffset(&ChainModifierDesc::RuntimeSettings::windWeightScale), 0.01f, -1.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "runtime.constraintDeadzone", "Const Dead", ChainRuntimeOffset(&ChainModifierDesc::RuntimeSettings::constraintDeadzone), 0.001f, 0.f, 10.f);

			return result;
		}();

	return &info;
}

void ChainModifier::On_RuntimeReflectionChanged(ModelCom *model, const PropertyInfo &property)
{
	UNREFERENCED_PARAMETER(model);
	UNREFERENCED_PARAMETER(property);
}

void ChainModifier::RuntimeRebuild(ModelCom *model)
{
	if (!model)
	{
		return;
	}

	_chains.clear();
	_chainsInitialized = false;
	Initialize_Chains(model);
	_isFirstUpdate = true;
	ResetPose(model);
}

string ChainModifier::Get_RuntimeGroupRootBoneName(size_t groupIndex) const
{
	if (groupIndex >= _desc.groups.size())
	{
		return {};
	}

	return _desc.groups[groupIndex].rootBoneName;
}

Vec3 ChainModifier::Get_RuntimeGroupRootRotationOffset(size_t groupIndex) const
{
	if (groupIndex >= _desc.groups.size())
	{
		return Vec3::Zero;
	}

	return _desc.groups[groupIndex].rootRotationOffsetDegrees;
}

void ChainModifier::Set_RuntimeGroupRootRotationOffset(size_t groupIndex, const Vec3 &offsetDegrees)
{
	if (groupIndex >= _desc.groups.size())
	{
		return;
	}

	_desc.groups[groupIndex].rootRotationOffsetDegrees = offsetDegrees;

	const string &rootBoneName = _desc.groups[groupIndex].rootBoneName;
	for (auto &chain : _chains)
	{
		if (chain.rootBoneName != rootBoneName)
		{
			continue;
		}

		chain.rootRotationOffsetDegrees = offsetDegrees;
		break;
	}
}

void ChainModifier::Set_RuntimeTestOwnerMotion(bool enabled, const Vec3& delta)
{
	_runtimeTestOwnerMotionEnabled = enabled;
	_runtimeTestOwnerMotionDelta = enabled ? delta : Vec3::Zero;
}

void ChainModifier::Capture_CurrentPose(ModelCom *model, const Matrix &ownerWorld)
{
	for (auto &chain : _chains)
	{
		for (auto &node : chain.nodes)
		{
			node.prevOriginalWorld = node.originalWorld;
			node.prevOriginalPosition = node.originalWorld.Translation();
		}

		if (_desc.blendMode == ModifierBlendMode::ModifyAddictive)
		{
			Capture_AdditivePose(model, chain, ownerWorld);
		}
		else
		{
			Capture_ReplacePose(model, chain, ownerWorld);
		}

		Update_PoseLocalDirections(chain);
	}
}

void ChainModifier::Apply_OwnerTranslationToSimulation(const Vec3 &ownerDelta)
{
	if (!_desc.motion.followOwnerTranslationInSimulation || ownerDelta.LengthSquared() <= 0.000001f)
	{
		return;
	}

	for (auto &chain : _chains)
	{
		for (auto &node : chain.nodes)
		{
			if (node.isVirtualAnchor)
			{
				continue;
			}

			node.position += ownerDelta;
			node.prevPosition += ownerDelta;
		}
	}
}

void ChainModifier::Capture_ReplacePose(ModelCom *model, BoneChain &chain, const Matrix &ownerWorld)
{
	for (size_t i = 0; i < chain.nodes.size(); ++i)
	{
		auto &node = chain.nodes[i];
		if (node.isVirtualAnchor)
		{
			const Matrix *combinedMat = model->Get_BoneMatrixPtr(node.boneIndex);
			node.poseLocal = node.bindLocal;
			node.originalWorld = combinedMat ? (*combinedMat) * ownerWorld : ownerWorld;
			continue;
		}

		if (node.isVirtualTip)
		{
			auto &parentNode = chain.nodes[node.parentNodeIndex];
			Vec3 localTipDir(0.f, -1.f, 0.f);
			localTipDir.Normalize();
			node.poseLocal = Matrix::Identity;
			Vec3 worldTipDir = Get_WorldDirection(parentNode.originalWorld, localTipDir);
			if (worldTipDir.LengthSquared() <= 0.0001f)
			{
				worldTipDir = Vec3::UnitY;
			}

			worldTipDir.Normalize();
			node.originalWorld = BuildTranslationMatrix(parentNode.originalWorld.Translation() + worldTipDir * node.length);
			continue;
		}

		node.poseLocal = model->Get_BoneLocalMatrix(node.boneIndex);
		const bool applyRootOffset = node.parentNodeIndex >= 0
			&& chain.nodes[node.parentNodeIndex].isVirtualAnchor;
		const Vec3 rootOffsetDegrees = chain.rootRotationOffsetDegrees;
		if (applyRootOffset && rootOffsetDegrees.LengthSquared() > 0.0001f)
		{
			node.poseLocal = Build_LocalMatrix_RotationOnly(node.poseLocal, BuildRootOffsetQuaternion(rootOffsetDegrees));
		}

		if (node.parentNodeIndex >= 0)
		{
			const Matrix &parentWorld = chain.nodes[node.parentNodeIndex].originalWorld;
			node.originalWorld = node.poseLocal * parentWorld;
			continue;
		}

		node.originalWorld = node.poseLocal * ownerWorld;
	}
}

void ChainModifier::Capture_AdditivePose(ModelCom *model, BoneChain &chain, const Matrix &ownerWorld)
{
	for (auto &node : chain.nodes)
	{
		if (node.isVirtualTip)
		{
			auto &parentNode = chain.nodes[node.parentNodeIndex];
			Vec3 localTipDir(0.f, -1.f, 0.f);
			localTipDir.Normalize();
			node.poseLocal = Matrix::Identity;
			Vec3 worldTipDir = Get_WorldDirection(parentNode.originalWorld, localTipDir);
			if (worldTipDir.LengthSquared() <= 0.0001f)
			{
				worldTipDir = Vec3::UnitY;
			}

			worldTipDir.Normalize();
			node.originalWorld = BuildTranslationMatrix(parentNode.originalWorld.Translation() + worldTipDir * node.length);
			continue;
		}

		const Matrix *combinedMat = model->Get_BoneMatrixPtr(node.boneIndex);
		node.poseLocal = model->Get_BoneLocalMatrix(node.boneIndex);
		const bool applyRootOffset = node.parentNodeIndex >= 0
			&& chain.nodes[node.parentNodeIndex].isVirtualAnchor;
		const Vec3 rootOffsetDegrees = chain.rootRotationOffsetDegrees;
		if (applyRootOffset && rootOffsetDegrees.LengthSquared() > 0.0001f)
		{
			node.poseLocal = Build_LocalMatrix_RotationOnly(node.poseLocal, BuildRootOffsetQuaternion(rootOffsetDegrees));
		}

		if (node.parentNodeIndex >= 0)
		{
			const Matrix &parentWorld = chain.nodes[node.parentNodeIndex].originalWorld;
			node.originalWorld = node.poseLocal * parentWorld;
		}
		else
		{
			node.originalWorld = combinedMat ? (*combinedMat) * ownerWorld : ownerWorld;
		}
	}
}

void ChainModifier::Update_PoseLocalDirections(BoneChain &chain)
{
	for (auto &node : chain.nodes)
	{
		node.poseLocalDir = Vec3::Zero;
		if (node.parentNodeIndex < 0 || node.isVirtualAnchor)
		{
			continue;
		}

		auto &parentNode = chain.nodes[node.parentNodeIndex];
		Vec3 poseWorldDir = node.originalWorld.Translation() - parentNode.originalWorld.Translation();
		if (poseWorldDir.LengthSquared() <= 0.0001f)
		{
			continue;
		}

		node.poseLocalDir = Get_LocalDirection(parentNode.originalWorld, poseWorldDir);
	}
}

void ChainModifier::Solve_Verlet(ModelCom *model, float timeDelta, const Matrix &ownerWorld, const Vec3 &ownerMotionDelta, const Vec3 &poseInertiaOwnerDelta, float movementWeight)
{
	const float delta = std::clamp(timeDelta, 0.f, 1.f / 30.f);
	vector<WorldCapsule> worldCapsules = _desc.collision.enableCollision ? Update_WorldCapsule(model, &ownerWorld) : vector<WorldCapsule>{};
	const float constraintDeadzone = std::max(_desc.runtime.constraintDeadzone, 0.f);
	const float constraintDeadzoneSq = constraintDeadzone * constraintDeadzone;
	const bool useConstraintDeadzone = constraintDeadzoneSq > 0.f;
	const auto applyConstraintTarget = [useConstraintDeadzone, constraintDeadzoneSq](BoneChainNode &node, const Vec3 &target)
		{
			const Vec3 correction = target - node.position;
			if (useConstraintDeadzone && correction.LengthSquared() <= constraintDeadzoneSq)
			{
				return;
			}

			node.position = target;
		};

	if (_desc.collision.debugDrawCapsules)
	{
		for (const WorldCapsule &capsule : worldCapsules)
		{
			DebugCapsuleDesc debugDesc{};
			debugDesc.start = capsule.startPoint;
			debugDesc.end = capsule.endPoint;
			debugDesc.radius = capsule.radius;
			debugDesc.style.color = Color(1.f, 0.9f, 0.05f, 1.f);
			debugDesc.style.depthEnabled = false;
			GAME->Draw_DebugCapsule(debugDesc);
		}
	}

	for (auto &chain : _chains)
	{
		const float shapeStiffness = std::clamp(_desc.shape.stiffness * chain.stiffnessMul, 0.f, 1.f);
		float curDamping = _desc.motion.damping * chain.dampingMul;
		Vec3 curGravity = _desc.runtime.gravity * chain.gravityMul;
		if (_desc.motion.usePurseModifier)
		{
			chain.purseModifier.Prepare(chain);
		}

		const auto isPinnedNode = [this, &chain](const BoneChainNode &node)
			{
				if (node.parentNodeIndex == -1 || node.isVirtualAnchor)
				{
					return true;
				}

				return _desc.shape.pinRoot && chain.nodes[node.parentNodeIndex].isVirtualAnchor;
			};

		for (size_t i = 0; i < chain.nodes.size(); ++i)
		{
			auto &node = chain.nodes[i];

			if (isPinnedNode(node))
			{
				node.position = node.originalWorld.Translation();
				node.prevPosition = node.position;
				continue;
			}

			const float tipRatio = GetChainTipRatio(chain, i);

			const float nodeShapeStiffness = std::clamp(
				BlendTipScale(shapeStiffness, _desc.shape.tipStiffnessScale, tipRatio),
				0.f,
				1.f);
			const float nodeDamping = std::clamp(
				BlendTipScale(curDamping, _desc.motion.tipDampingScale, tipRatio),
				0.f,
				1.f);
			const float nodeMovementInertiaScale = BlendTipScale(
				_desc.motion.movementInertiaScale,
				_desc.motion.tipMovementInertiaScale,
				tipRatio);

			float windTimeScale = 1.f;
			if (_desc.runtime.targetFrameRate > 1.f)
			{
				windTimeScale = std::clamp(delta * _desc.runtime.targetFrameRate, 0.f, 4.f);
			}

			Vec3 externalOffset = ComputeWindOffset(_desc, chain, node, i) * windTimeScale;
			if (nodeMovementInertiaScale > 0.f)
			{
				Vec3 poseInertiaDelta = node.originalWorld.Translation() - node.prevOriginalPosition;
				if (_desc.motion.removeOwnerMotionFromPoseInertia)
				{
					poseInertiaDelta -= poseInertiaOwnerDelta;
				}
				externalOffset += -poseInertiaDelta * nodeMovementInertiaScale * chain.movementInertiaMul * movementWeight;
			}
			if (_desc.motion.usePurseModifier && _desc.motion.ownerMovementInertiaScale > 0.f)
			{
				const float purseTipScale = BlendTipScale(1.f, _desc.motion.tipMovementInertiaScale, tipRatio);
				externalOffset += chain.purseModifier.Compute_RootMotionOffset(chain, i)
					* _desc.motion.ownerMovementInertiaScale
					* purseTipScale
					* chain.movementInertiaMul
					* movementWeight;
			}
			else if (_desc.motion.ownerMovementInertiaScale > 0.f && ownerMotionDelta.LengthSquared() > 0.000001f)
			{
				externalOffset += -ownerMotionDelta * _desc.motion.ownerMovementInertiaScale * chain.movementInertiaMul * movementWeight;
			}

			const float movementOffsetLength = externalOffset.Length();
			const float maxMovementOffset = _desc.motion.maxMovementOffset * movementWeight;
			if (movementOffsetLength > maxMovementOffset && movementOffsetLength > 0.0001f)
			{
				externalOffset /= movementOffsetLength;
				externalOffset *= maxMovementOffset;
			}

			Vec3 nextPosition = Math::SolveVerlet(node.position, node.prevPosition, curGravity, nodeDamping, delta);
			nextPosition += externalOffset;

			node.prevPosition = node.position;
			node.position = nextPosition;

			if (_desc.shape.poseRestoreStrength > 0.f && nodeShapeStiffness > 0.f)
			{
				const float nodePoseRestoreStrength = BlendTipScale(
					_desc.shape.poseRestoreStrength,
					_desc.shape.tipPoseRestoreScale,
					tipRatio);
				const float restoreAlpha = std::clamp(nodePoseRestoreStrength * chain.poseRestoreMul * nodeShapeStiffness, 0.f, 1.f);
				const Matrix &parentBasisWorld = chain.nodes[node.parentNodeIndex].originalWorld;
				const Vec3& restLocalDir = _desc.shape.useBindPoseAsRest ? node.bindLocalDir : node.poseLocalDir;
				const Vec3 restWorldDir = Get_WorldDirection(parentBasisWorld, restLocalDir);
				if (restWorldDir.LengthSquared() > 0.0001f)
				{
					const Vec3 restTarget = chain.nodes[node.parentNodeIndex].position + restWorldDir * node.length;
					applyConstraintTarget(node, Vec3::Lerp(node.position, restTarget, restoreAlpha));
				}
			}

			if (_desc.collision.enableCollision)
			{
				node.position = Compute_WorldCapsule(node, worldCapsules, node.position);
			}
		}

		for (size_t i = 0; i < chain.nodes.size(); ++i)
		{
			auto &node = chain.nodes[i];
			if (isPinnedNode(node))
			{
				continue;
			}

			auto &parentNode = chain.nodes[node.parentNodeIndex];
			Vec3 parentToNode = node.position - parentNode.position;
			const float currentDist = parentToNode.Length();

			if (currentDist > 0.0001f)
			{
				applyConstraintTarget(node, parentNode.position + (parentToNode / currentDist) * node.length);
			}

			if (_desc.collision.enableCollision)
			{
				node.position = Compute_WorldCapsule(node, worldCapsules, node.position);
			}
		}

		for (size_t i = 0; i < chain.nodes.size(); ++i)
		{
			auto &node = chain.nodes[i];
			if (isPinnedNode(node))
			{
				continue;
			}

			auto &parentNode = chain.nodes[node.parentNodeIndex];
			Vec3 simulatedDir = node.position - parentNode.position;
			const Vec3& restLocalDir = _desc.shape.useBindPoseAsRest ? node.bindLocalDir : node.poseLocalDir;
			if (restLocalDir.LengthSquared() <= 0.0001f || simulatedDir.LengthSquared() <= 0.0001f)
			{
				continue;
			}

			const float tipRatio = GetChainTipRatio(chain, i);
			const float nodeShapeStiffness = std::clamp(
				BlendTipScale(shapeStiffness, _desc.shape.tipStiffnessScale, tipRatio),
				0.f,
				1.f);
			const float nodeMaxBendAngleDegree = BlendTipScale(
				_desc.shape.maxBendAngleDegree,
				_desc.shape.tipMaxBendAngleScale,
				tipRatio) * chain.maxBendAngleMul;

			simulatedDir.Normalize();
			const Vec3 simulatedLocalDir = Get_LocalDirection(parentNode.originalWorld, simulatedDir);
			if (simulatedLocalDir.LengthSquared() <= 0.0001f)
			{
				continue;
			}

			const Vec3 limitedLocalDir = Limit_LocalDirectionByAngle(
				restLocalDir,
				simulatedLocalDir,
				nodeMaxBendAngleDegree);
			const Vec3 limitedWorldDir = Get_WorldDirection(parentNode.originalWorld, limitedLocalDir);
			if (limitedWorldDir.LengthSquared() > 0.0001f)
			{
				const Vec3 limitedTarget = parentNode.position + limitedWorldDir * node.length;
				applyConstraintTarget(node, Vec3::Lerp(node.position, limitedTarget, nodeShapeStiffness));
			}

			if (_desc.collision.enableCollision)
			{
				node.position = Compute_WorldCapsule(node, worldCapsules, node.position);
			}
		}
	}

	Solve_SurfaceConstraints(worldCapsules);

	// Surface constraint가 좌우 체인을 벌린 뒤에는 세그먼트 길이/꺾임이 다시 틀어질 수 있어서
	// 코트가 종이처럼 접힌 모양으로 남지 않도록 한 번 더 안정화한다.
	for (auto &chain : _chains)
	{
		const float shapeStiffness = std::clamp(_desc.shape.stiffness * chain.stiffnessMul, 0.f, 1.f);
		const auto isPinnedNode = [this, &chain](const BoneChainNode &node)
			{
				if (node.parentNodeIndex == -1 || node.isVirtualAnchor)
				{
					return true;
				}

				return _desc.shape.pinRoot && chain.nodes[node.parentNodeIndex].isVirtualAnchor;
			};

		for (size_t i = 0; i < chain.nodes.size(); ++i)
		{
			auto &node = chain.nodes[i];
			if (isPinnedNode(node))
			{
				continue;
			}

			auto &parentNode = chain.nodes[node.parentNodeIndex];
			Vec3 parentToNode = node.position - parentNode.position;
			const float currentDist = parentToNode.Length();
			if (currentDist > 0.0001f)
			{
				applyConstraintTarget(node, parentNode.position + (parentToNode / currentDist) * node.length);
			}

			if (_desc.collision.enableCollision)
			{
				node.position = Compute_WorldCapsule(node, worldCapsules, node.position);
			}
		}

		for (size_t i = 0; i < chain.nodes.size(); ++i)
		{
			auto &node = chain.nodes[i];
			if (isPinnedNode(node))
			{
				continue;
			}

			auto &parentNode = chain.nodes[node.parentNodeIndex];
			Vec3 simulatedDir = node.position - parentNode.position;
			const Vec3& restLocalDir = _desc.shape.useBindPoseAsRest ? node.bindLocalDir : node.poseLocalDir;
			if (restLocalDir.LengthSquared() <= 0.0001f || simulatedDir.LengthSquared() <= 0.0001f)
			{
				continue;
			}

			const float tipRatio = GetChainTipRatio(chain, i);
			const float nodeShapeStiffness = std::clamp(
				BlendTipScale(shapeStiffness, _desc.shape.tipStiffnessScale, tipRatio),
				0.f,
				1.f);
			const float nodeMaxBendAngleDegree = BlendTipScale(
				_desc.shape.maxBendAngleDegree,
				_desc.shape.tipMaxBendAngleScale,
				tipRatio) * chain.maxBendAngleMul;

			simulatedDir.Normalize();
			const Vec3 simulatedLocalDir = Get_LocalDirection(parentNode.originalWorld, simulatedDir);
			if (simulatedLocalDir.LengthSquared() <= 0.0001f)
			{
				continue;
			}

			const Vec3 limitedLocalDir = Limit_LocalDirectionByAngle(
				restLocalDir,
				simulatedLocalDir,
				nodeMaxBendAngleDegree);
			const Vec3 limitedWorldDir = Get_WorldDirection(parentNode.originalWorld, limitedLocalDir);
			if (limitedWorldDir.LengthSquared() > 0.0001f)
			{
				const Vec3 limitedTarget = parentNode.position + limitedWorldDir * node.length;
				applyConstraintTarget(node, Vec3::Lerp(node.position, limitedTarget, nodeShapeStiffness));
			}

			if (_desc.collision.enableCollision)
			{
				node.position = Compute_WorldCapsule(node, worldCapsules, node.position);
			}
		}
	}
}

void ChainModifier::Solve_SurfaceConstraints(const vector<WorldCapsule> &worldCapsules)
{
	const float constraintStrength = std::clamp(_desc.shape.surfaceConstraintStrength, 0.f, 1.f);
	if (constraintStrength <= 0.f || _chains.size() < 2)
	{
		return;
	}

	const auto isPinnedNode = [this](const BoneChain &chain, const BoneChainNode &node)
		{
			if (node.parentNodeIndex < 0 || node.isVirtualAnchor)
			{
				return true;
			}

			return _desc.shape.pinRoot && chain.nodes[node.parentNodeIndex].isVirtualAnchor;
		};

	const auto moveNode = [this, &worldCapsules](BoneChainNode &node, const Vec3 &offset)
		{
			node.position += offset;
			node.prevPosition += offset;

			if (_desc.collision.enableCollision)
			{
				const Vec3 correctedPosition = Compute_WorldCapsule(node, worldCapsules, node.position);
				const Vec3 collisionOffset = correctedPosition - node.position;
				node.position = correctedPosition;
				node.prevPosition += collisionOffset;
			}
		};

	for (size_t chainIndex = 0; chainIndex + 1 < _chains.size(); ++chainIndex)
	{
		auto &leftChain = _chains[chainIndex];
		auto &rightChain = _chains[chainIndex + 1];
		const size_t nodeCount = std::min(leftChain.nodes.size(), rightChain.nodes.size());
		for (size_t nodeIndex = 1; nodeIndex < nodeCount; ++nodeIndex)
		{
			auto &leftNode = leftChain.nodes[nodeIndex];
			auto &rightNode = rightChain.nodes[nodeIndex];
			if (leftNode.isVirtualAnchor || rightNode.isVirtualAnchor)
			{
				continue;
			}

			const bool leftPinned = isPinnedNode(leftChain, leftNode);
			const bool rightPinned = isPinnedNode(rightChain, rightNode);
			if (leftPinned && rightPinned)
			{
				continue;
			}

			const Vec3 restDelta = rightNode.originalWorld.Translation() - leftNode.originalWorld.Translation();
			const float restDistance = restDelta.Length();
			if (restDistance <= 0.0001f)
			{
				continue;
			}

			Vec3 currentDelta = rightNode.position - leftNode.position;
			const float currentDistance = currentDelta.Length();
			if (currentDistance <= 0.0001f)
			{
				continue;
			}

			const float error = currentDistance - restDistance;
			const Vec3 correction = (currentDelta / currentDistance) * (error * constraintStrength);
			if (!leftPinned && !rightPinned)
			{
				moveNode(leftNode, correction * 0.5f);
				moveNode(rightNode, -correction * 0.5f);
			}
			else if (!leftPinned)
			{
				moveNode(leftNode, correction);
			}
			else if (!rightPinned)
			{
				moveNode(rightNode, -correction);
			}
		}
	}
}

Vec3 ChainModifier::Get_LocalDirection(const Matrix &parentWorld, const Vec3 &worldDirection)
{
	const Matrix parentInv = parentWorld.Invert();

	Vec3 localDirection = Vec3::TransformNormal(worldDirection, parentInv);
	if (localDirection.LengthSquared() > 0.0001f)
	{
		localDirection.Normalize();
	}

	return localDirection;
}

Vec3 ChainModifier::Get_WorldDirection(const Matrix &parentWorld, const Vec3 &localDirection)
{
	Vec3 worldDirection = Vec3::TransformNormal(localDirection, parentWorld);
	if (worldDirection.LengthSquared() > 0.0001f)
	{
		worldDirection.Normalize();
	}

	return worldDirection;
}

Vec3 ChainModifier::Limit_LocalDirectionByAngle(const Vec3 &bindLocalDirection, const Vec3 &simulatedLocalDirection, float maxAngleDegree)
{
	Vec3 bindDir = bindLocalDirection;
	Vec3 simDir = simulatedLocalDirection;

	if (bindDir.LengthSquared() <= 0.0001f || simDir.LengthSquared() <= 0.0001f)
	{
		return bindLocalDirection;
	}

	bindDir.Normalize();
	simDir.Normalize();

	if (maxAngleDegree <= 0.f)
	{
		return simDir;
	}

	const float maxAngle = XMConvertToRadians(maxAngleDegree);
	float dot = std::clamp(bindDir.Dot(simDir), -1.f, 1.f);
	const float angle = acosf(dot);
	if (angle <= maxAngle)
	{
		return simDir;
	}

	Vec3 axis = bindDir.Cross(simDir);
	if (axis.LengthSquared() <= 0.0001f)
	{
		return bindDir;
	}

	axis.Normalize();

	const Quaternion limitedRotation = Quaternion::CreateFromAxisAngle(axis, maxAngle);
	Vec3 limitedDir = Vec3::Transform(bindDir, Matrix::CreateFromQuaternion(limitedRotation));
	if (limitedDir.LengthSquared() > 0.0001f)
	{
		limitedDir.Normalize();
	}

	return limitedDir;
}

Quaternion ChainModifier::Build_RotationDelta(const Vec3 &fromDirection, const Vec3 &toDirection)
{
	Vec3 from = fromDirection;
	Vec3 to = toDirection;

	if (from.LengthSquared() <= 0.0001f || to.LengthSquared() <= 0.0001f)
	{
		return Quaternion::Identity;
	}

	from.Normalize();
	to.Normalize();

	float dot = std::clamp(from.Dot(to), -1.f, 1.f);
	if (dot > 0.9995f)
	{
		return Quaternion::Identity;
	}

	if (dot < -0.9995f)
	{
		Vec3 axis = Vec3::UnitX.Cross(from);
		if (axis.LengthSquared() <= 0.0001f)
		{
			axis = Vec3::UnitY.Cross(from);
		}

		if (axis.LengthSquared() <= 0.0001f)
		{
			return Quaternion::Identity;
		}

		axis.Normalize();
		return Quaternion::CreateFromAxisAngle(axis, XM_PI);
	}

	Vec3 axis = from.Cross(to);
	if (axis.LengthSquared() <= 0.0001f)
	{
		return Quaternion::Identity;
	}

	axis.Normalize();

	const float angle = acosf(dot);
	Quaternion q = Quaternion::CreateFromAxisAngle(axis, angle);
	q.Normalize();

	return q;
}

Matrix ChainModifier::Build_LocalMatrix_RotationOnly(const Matrix &bindLocal, const Quaternion &deltaRotation)
{
	Matrix tempMatrix = bindLocal;

	Vec3 scale{};
	Quaternion bindRotation{};
	Vec3 translation{};
	tempMatrix.Decompose(scale, bindRotation, translation);

	Quaternion finalRotation = deltaRotation * bindRotation;
	finalRotation.Normalize();

	Matrix local = Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(finalRotation);
	local._41 = bindLocal._41;
	local._42 = bindLocal._42;
	local._43 = bindLocal._43;
	local._44 = bindLocal._44;

	return local;
}

void ChainModifier::Write_BackToModel(ModelCom *model, const Matrix &ownerWorld)
{
	UNREFERENCED_PARAMETER(ownerWorld);

	if (!model)
	{
		return;
	}

	for (auto &chain : _chains)
	{
		Write_BackChain(model, chain);
	}
}

void ChainModifier::Write_BackChain(ModelCom *model, BoneChain &chain)
{
	const float shapeStiffness = std::clamp(_desc.shape.stiffness * chain.stiffnessMul, 0.f, 1.f);
	const float outputAlpha = std::clamp(_strengthAlpha, 0.f, 1.f);
	const float rootOutputAlpha = std::clamp(_desc.shape.rootOutputAlpha, 0.f, 1.f);
	const float tipOutputAlpha = std::clamp(_desc.shape.tipOutputAlpha, 0.f, 1.f);
	const auto isRootNode = [this, &chain](const BoneChainNode &node)
		{
			return _desc.shape.pinRoot && node.parentNodeIndex >= 0 && chain.nodes[node.parentNodeIndex].isVirtualAnchor;
		};
	const auto isChainRootNode = [&chain](const BoneChainNode &node)
		{
			return node.parentNodeIndex >= 0 && chain.nodes[node.parentNodeIndex].isVirtualAnchor;
		};

	for (size_t i = 0; i < chain.nodes.size(); ++i)
	{
		auto &node = chain.nodes[i];
		if (!isRootNode(node))
		{
			continue;
		}

		Matrix baseLocal = _desc.shape.useBindPoseAsRest ? node.bindLocal : node.poseLocal;
		if (node.childNodeIndices.empty())
		{
			model->Set_BoneLocalMatrix(node.boneIndex, baseLocal);
			continue;
		}

		auto &firstChild = chain.nodes[node.childNodeIndices.front()];
		Vec3 simulatedWorldDir = firstChild.position - node.position;
		const Vec3& firstChildRestLocalDir = _desc.shape.useBindPoseAsRest ? firstChild.bindLocalDir : firstChild.poseLocalDir;
		if (simulatedWorldDir.LengthSquared() <= 0.0001f || firstChildRestLocalDir.LengthSquared() <= 0.0001f)
		{
			model->Set_BoneLocalMatrix(node.boneIndex, baseLocal);
			continue;
		}

		simulatedWorldDir.Normalize();
		const Vec3 simulatedLocalDir = Get_LocalDirection(node.originalWorld, simulatedWorldDir);
		if (simulatedLocalDir.LengthSquared() <= 0.0001f)
		{
			model->Set_BoneLocalMatrix(node.boneIndex, baseLocal);
			continue;
		}

		const size_t childNodeIndex = static_cast<size_t>(node.childNodeIndices.front());
		const float tipRatio = GetChainTipRatio(chain, childNodeIndex);
		const float nodeShapeStiffness = std::clamp(
			BlendTipScale(shapeStiffness, _desc.shape.tipStiffnessScale, tipRatio),
			0.f,
			1.f);
		const float nodeMaxBendAngleDegree = BlendTipScale(
			_desc.shape.maxBendAngleDegree,
			_desc.shape.tipMaxBendAngleScale,
			tipRatio) * chain.maxBendAngleMul;

		const Vec3 limitedLocalDir = Limit_LocalDirectionByAngle(firstChildRestLocalDir, simulatedLocalDir, nodeMaxBendAngleDegree);
		Vec3 blendedLocalDir = Vec3::Lerp(simulatedLocalDir, limitedLocalDir, nodeShapeStiffness);
		if (blendedLocalDir.LengthSquared() > 0.0001f)
		{
			blendedLocalDir.Normalize();
		}
		Quaternion deltaRotation = Build_RotationDelta(firstChildRestLocalDir, blendedLocalDir);
		deltaRotation = Quaternion::Slerp(Quaternion::Identity, deltaRotation, outputAlpha * rootOutputAlpha);
		deltaRotation.Normalize();
		model->Set_BoneLocalMatrix(node.boneIndex, Build_LocalMatrix_RotationOnly(baseLocal, deltaRotation));
	}
	model->Rebuild_BoneCombinedMatrices();

	for (size_t i = 0; i < chain.nodes.size(); ++i)
	{
		auto &node = chain.nodes[i];
		if (node.isVirtualAnchor || node.isVirtualTip || node.parentNodeIndex < 0 || isChainRootNode(node))
		{
			continue;
		}

		auto &parentNode = chain.nodes[node.parentNodeIndex];
		const Matrix *parentWorld = model->Get_BoneMatrixPtr(parentNode.boneIndex);
		if (!parentWorld)
		{
			continue;
		}

		Vec3 simulatedWorldDir = node.position - parentNode.position;
		if (simulatedWorldDir.LengthSquared() <= 0.0001f)
		{
			continue;
		}

		simulatedWorldDir.Normalize();
		const Vec3 simulatedLocalDir = Get_LocalDirection(*parentWorld, simulatedWorldDir);
		const Vec3& restLocalDir = _desc.shape.useBindPoseAsRest ? node.bindLocalDir : node.poseLocalDir;
		if (restLocalDir.LengthSquared() <= 0.0001f || simulatedLocalDir.LengthSquared() <= 0.0001f)
		{
			continue;
		}

		const float tipRatio = GetChainTipRatio(chain, i);
		const float nodeShapeStiffness = std::clamp(
			BlendTipScale(shapeStiffness, _desc.shape.tipStiffnessScale, tipRatio),
			0.f,
			1.f);
		const float nodeOutputAlpha = std::clamp(std::lerp(rootOutputAlpha, tipOutputAlpha, tipRatio), 0.f, 1.f);
		const float nodeMaxBendAngleDegree = BlendTipScale(
			_desc.shape.maxBendAngleDegree,
			_desc.shape.tipMaxBendAngleScale,
			tipRatio) * chain.maxBendAngleMul;

		const Vec3 limitedLocalDir = Limit_LocalDirectionByAngle(restLocalDir, simulatedLocalDir, nodeMaxBendAngleDegree);
		Vec3 blendedLocalDir = Vec3::Lerp(simulatedLocalDir, limitedLocalDir, nodeShapeStiffness);
		if (blendedLocalDir.LengthSquared() > 0.0001f)
		{
			blendedLocalDir.Normalize();
		}
		Quaternion deltaRotation = Build_RotationDelta(restLocalDir, blendedLocalDir);
		deltaRotation = Quaternion::Slerp(Quaternion::Identity, deltaRotation, outputAlpha * nodeOutputAlpha);
		deltaRotation.Normalize();
		const Matrix& baseLocal = _desc.shape.useBindPoseAsRest ? node.bindLocal : node.poseLocal;
		model->Set_BoneLocalMatrix(node.boneIndex, Build_LocalMatrix_RotationOnly(baseLocal, deltaRotation));
		model->Rebuild_BoneCombinedMatrices();
	}
}

vector<ChainModifier::WorldCapsule> ChainModifier::Update_WorldCapsule(const ModelCom *model, const Matrix *ownerWorld)
{
	vector<WorldCapsule> worldCapsules;
	worldCapsules.reserve(_colliders.size());
	bool loggedDebugCapsule = false;

	for (const auto &col : _colliders)
	{
		if (!_desc.collision.includedBones.empty()
			&& std::find(_desc.collision.includedBones.begin(), _desc.collision.includedBones.end(), col.boneName) == _desc.collision.includedBones.end())
		{
			continue;
		}

		int32 boneIndex = model->Get_BoneIndex(col.boneName);
		if (boneIndex == -1)
		{
			continue;
		}

		const Matrix *boneCombined = model->Get_BoneMatrixPtr(boneIndex);
		if (!boneCombined)
		{
			continue;
		}

		Matrix boneWorld = (*boneCombined) * (*ownerWorld);

		Vec3 localAxis = Vec3::Transform(Vec3::UnitY, col.rotationQuat);
		if (localAxis.LengthSquared() <= 0.0001f)
		{
			localAxis = Vec3::UnitY;
		}
		localAxis.Normalize();

		Vec3 worldScale = Vec3::One;
		Quaternion worldRotation = Quaternion::Identity;
		Vec3 worldTranslation = Vec3::Zero;
		Matrix decomposedBoneWorld = boneWorld;
		decomposedBoneWorld.Decompose(worldScale, worldRotation, worldTranslation);

		const Matrix boneRotationWorld = Matrix::CreateFromQuaternion(worldRotation);
		Vec3 worldAxis = Vec3::TransformNormal(localAxis, boneRotationWorld);
		if (worldAxis.LengthSquared() <= 0.0001f)
		{
			worldAxis = Vec3::UnitY;
		}
		worldAxis.Normalize();

		Vec3 scaledLocalAxis{
			localAxis.x * fabsf(worldScale.x),
			localAxis.y * fabsf(worldScale.y),
			localAxis.z * fabsf(worldScale.z)
		};
		float lengthScale = scaledLocalAxis.Length();
		if (lengthScale <= 0.0001f)
		{
			lengthScale = 1.f;
		}

		const Vec3 worldCenter = Vec3::Transform(col.center, boneWorld);
		const float radiusScale = std::max(std::max(fabsf(worldScale.x), fabsf(worldScale.y)), fabsf(worldScale.z));

		WorldCapsule capsule{};
		capsule.radius = col.radius * radiusScale;
		float scaledHalfLength = col.halfLength * lengthScale;

		capsule.startPoint = worldCenter + worldAxis * scaledHalfLength;
		capsule.endPoint = worldCenter - worldAxis * scaledHalfLength;

		if (_desc.collision.debugDrawCapsules && !loggedDebugCapsule)
		{
			LOG_INFO_THROTTLED(
				120,
				"[ChainModifier][Capsule] tag={} bone={} center=({:.2f},{:.2f},{:.2f}) axis=({:.2f},{:.2f},{:.2f}) half={:.2f} radius={:.2f}",
				String::ToString(_desc.modifierTag),
				col.boneName,
				worldCenter.x,
				worldCenter.y,
				worldCenter.z,
				worldAxis.x,
				worldAxis.y,
				worldAxis.z,
				scaledHalfLength,
				capsule.radius);
			loggedDebugCapsule = true;
		}

		worldCapsules.push_back(capsule);
	}

	return worldCapsules;
}

Vec3 ChainModifier::Compute_WorldCapsule(BoneChainNode &currentNode, const vector<WorldCapsule> &capsules, const Vec3 &nextPosition)
{
	Vec3 computePos = nextPosition;

	for (auto &wc : capsules)
	{
		Vec3 lineVector = wc.endPoint - wc.startPoint;
		float lineLengthSq = lineVector.LengthSquared();

		float t = 0.0f;
		if (lineLengthSq > 0.0001f)
		{
			t = (computePos - wc.startPoint).Dot(lineVector) / lineLengthSq;
			t = std::clamp(t, 0.f, 1.f);
		}

		Vec3 closestPoint = wc.startPoint + lineVector * t;
		Vec3 toNode = computePos - closestPoint;
		float distSq = toNode.LengthSquared();

		const float minCollisionDist = wc.radius
			+ _desc.collision.particleCollisionRadius
			+ _desc.collision.collisionRadiusPadding;
		if (distSq >= minCollisionDist * minCollisionDist)
		{
			continue;
		}

		Vec3 pushNormal = toNode;
		if (pushNormal.LengthSquared() <= 0.0001f)
		{
			pushNormal = currentNode.originalWorld.Translation() - closestPoint;
		}

		if (pushNormal.LengthSquared() <= 0.0001f)
		{
			pushNormal = Vec3::UnitY;
		}

		const float dist = pushNormal.Length();
		if (dist >= minCollisionDist)
		{
			continue;
		}

		pushNormal /= dist;

		Vec3 offset = pushNormal * (minCollisionDist - dist);
		computePos += offset;
		currentNode.prevPosition += offset;
	}

	return computePos;
}

void ChainModifier::On_ActiveChanged(ModelCom *model, bool isActive)
{
	if (isActive && model)
	{
		_isFirstUpdate = true;
		if (_chainsInitialized)
		{
			ResetPose(model);
		}
	}
}

void ChainModifier::Evaluate_Modifier(ModelCom *model, float timeDelta)
{
	if (!_isActive || _strengthAlpha <= 0.f || !model)
	{
		return;
	}

	model->Rebuild_BoneCombinedMatrices();

	if (!_chainsInitialized)
	{
		Initialize_Chains(model);
	}

	Matrix ownerWorld = Resolve_OwnerWorldMatrix();
	Vec3 currentOwnerPos = ownerWorld.Translation();

	if (_isFirstUpdate)
	{
		_prevOwnerPos = currentOwnerPos;
		ResetPose(model);
		_isFirstUpdate = false;
		return;
	}

	const float ownerDistance = Vec3::Distance(_prevOwnerPos, currentOwnerPos);
	Vec3 poseInertiaOwnerDelta = currentOwnerPos - _prevOwnerPos;
	Vec3 ownerMotionDelta = poseInertiaOwnerDelta;
	if (_runtimeTestOwnerMotionEnabled)
	{
		ownerMotionDelta += _runtimeTestOwnerMotionDelta;
	}
	if (ownerDistance > _desc.runtime.resetDistance)
	{
		ResetPose(model);
		ownerMotionDelta = Vec3::Zero;
		poseInertiaOwnerDelta = Vec3::Zero;
	}

	_prevOwnerPos = currentOwnerPos;

	Capture_CurrentPose(model, ownerWorld);
	Apply_OwnerTranslationToSimulation(poseInertiaOwnerDelta);

	uint32 solverIterations = std::max<uint32>(_desc.motion.solverIterations, 1);
	if (_desc.runtime.targetFrameRate > 1.f)
	{
		const float maxSubstepDelta = 1.f / _desc.runtime.targetFrameRate;
		const uint32 targetIterations = static_cast<uint32>(std::ceil(std::max(timeDelta, 0.f) / maxSubstepDelta));
		solverIterations = std::max(solverIterations, std::max<uint32>(targetIterations, 1));
	}
	const float solverTimeDelta = timeDelta / static_cast<float>(solverIterations);
	const float movementWeight = 1.f / static_cast<float>(solverIterations);

	for (uint32 i = 0; i < solverIterations; ++i)
	{
		Solve_Verlet(model, solverTimeDelta, ownerWorld, ownerMotionDelta, poseInertiaOwnerDelta, movementWeight);
	}

	Write_BackToModel(model, ownerWorld);

	model->Rebuild_BoneCombinedMatrices();
}

Shared<ChainModifier> ChainModifier::Create(const ChainModifierDesc *desc)
{
	Shared<ChainModifier> instance = make_shared<ChainModifier>();

	if (FAILED(instance->Initialize(const_cast<ChainModifierDesc *>(desc))))
	{
		assert(false && "Failed to Create ChainModifier");
		return nullptr;
	}

	return instance;
}

void ChainModifier::Free()
{
	AnimationModifier::Free();
}

NS_END
