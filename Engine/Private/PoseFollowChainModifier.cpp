#include "PoseFollowChainModifier.h"
#include "ModelCom.h"
#include "ModelPhysicsParser.h"
#include "Reflection_Types.h"
#include "GameInstance.h"
#include "Engine_Macro.h"
#include <algorithm>
#include <cmath>

NS_BEGIN(Engine)

namespace
{
	template<typename Member>
	size_t PoseFollowOffset(Member PoseFollowChainModifier::PoseFollowChainModifierDesc::* member)
	{
		PoseFollowChainModifier::PoseFollowChainModifierDesc desc{};
		const auto* base = reinterpret_cast<const char*>(&desc);
		const auto* field = reinterpret_cast<const char*>(&(desc.*member));
		return static_cast<size_t>(field - base);
	}

	void AddRuntimeProperty(
		vector<PropertyInfo>& properties,
		PropertyType type,
		const string& name,
		const string& displayName,
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

	Matrix Get_OwnerWorld(const PoseFollowChainModifier::PoseFollowChainModifierDesc& desc)
	{
		if (desc.ownerWorldMatrix)
		{
			return *desc.ownerWorldMatrix;
		}

		return desc.ownerTransform ? desc.ownerTransform->Get_WorldMatrix() : Matrix::Identity;
	}

	float SmoothStep(float value)
	{
		value = std::clamp(value, 0.f, 1.f);
		return value * value * (3.f - 2.f * value);
	}

	float BlendTip(float rootValue, float tipValue, float tipRatio)
	{
		return std::lerp(rootValue, tipValue, tipRatio);
	}

	float GetAngleDegree(const Vec3& lhs, const Vec3& rhs)
	{
		Vec3 a = lhs;
		Vec3 b = rhs;
		if (a.LengthSquared() <= 0.0001f || b.LengthSquared() <= 0.0001f)
		{
			return 0.f;
		}

		a.Normalize();
		b.Normalize();
		return XMConvertToDegrees(acosf(std::clamp(a.Dot(b), -1.f, 1.f)));
	}

	float Get_TipRatio(size_t nodeIndex, size_t nodeCount)
	{
		if (nodeCount <= 1)
		{
			return 0.f;
		}

		const float nodeRatio = std::clamp(static_cast<float>(nodeIndex) / static_cast<float>(nodeCount - 1), 0.f, 1.f);
		return SmoothStep(nodeRatio);
	}

	Vec3 Get_LocalDirection(const Matrix& parentWorld, const Vec3& worldDirection)
	{
		Vec3 localDirection = Vec3::TransformNormal(worldDirection, parentWorld.Invert());
		if (localDirection.LengthSquared() > 0.0001f)
		{
			localDirection.Normalize();
		}

		return localDirection;
	}

	Vec3 Get_WorldDirection(const Matrix& parentWorld, const Vec3& localDirection)
	{
		Vec3 worldDirection = Vec3::TransformNormal(localDirection, parentWorld);
		if (worldDirection.LengthSquared() > 0.0001f)
		{
			worldDirection.Normalize();
		}

		return worldDirection;
	}

	Vec3 Limit_LocalDirectionByAngle(const Vec3& poseLocalDirection, const Vec3& simulatedLocalDirection, float maxAngleDegree)
	{
		Vec3 poseDir = poseLocalDirection;
		Vec3 simDir = simulatedLocalDirection;
		if (poseDir.LengthSquared() <= 0.0001f || simDir.LengthSquared() <= 0.0001f)
		{
			return poseLocalDirection;
		}

		poseDir.Normalize();
		simDir.Normalize();

		if (maxAngleDegree <= 0.f)
		{
			return poseDir;
		}

		const float maxAngle = XMConvertToRadians(maxAngleDegree);
		const float dot = std::clamp(poseDir.Dot(simDir), -1.f, 1.f);
		const float angle = acosf(dot);
		if (angle <= maxAngle)
		{
			return simDir;
		}

		Vec3 axis = poseDir.Cross(simDir);
		if (axis.LengthSquared() <= 0.0001f)
		{
			return poseDir;
		}

		axis.Normalize();
		Vec3 limitedDir = Vec3::Transform(poseDir, Matrix::CreateFromQuaternion(Quaternion::CreateFromAxisAngle(axis, maxAngle)));
		if (limitedDir.LengthSquared() > 0.0001f)
		{
			limitedDir.Normalize();
		}

		return limitedDir;
	}

	Quaternion Build_RotationDelta(const Vec3& fromDirection, const Vec3& toDirection)
	{
		Vec3 from = fromDirection;
		Vec3 to = toDirection;
		if (from.LengthSquared() <= 0.0001f || to.LengthSquared() <= 0.0001f)
		{
			return Quaternion::Identity;
		}

		from.Normalize();
		to.Normalize();

		const float dot = std::clamp(from.Dot(to), -1.f, 1.f);
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
		Quaternion delta = Quaternion::CreateFromAxisAngle(axis, acosf(dot));
		delta.Normalize();
		return delta;
	}

	Matrix Build_LocalMatrix_RotationOnly(const Matrix& baseLocal, const Quaternion& deltaRotation)
	{
		Matrix temp = baseLocal;
		Vec3 scale = Vec3::One;
		Quaternion baseRotation = Quaternion::Identity;
		Vec3 translation = Vec3::Zero;
		temp.Decompose(scale, baseRotation, translation);

		Quaternion finalRotation = deltaRotation * baseRotation;
		finalRotation.Normalize();

		Matrix local = Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(finalRotation);
		local._41 = baseLocal._41;
		local._42 = baseLocal._42;
		local._43 = baseLocal._43;
		local._44 = baseLocal._44;
		return local;
	}
}

HRESULT PoseFollowChainModifier::Initialize(AnimationModifierDesc* desc)
{
	if (FAILED(__super::Initialize(desc)))
	{
		return E_FAIL;
	}

	auto* poseDesc = static_cast<PoseFollowChainModifierDesc*>(desc);
	if (!poseDesc)
	{
		return E_FAIL;
	}

	_desc = *poseDesc;

	if (!_desc.overrideCapsules.empty())
	{
		_colliders = _desc.overrideCapsules;
	}
	else if (!_desc.physicsAssetPath.empty())
	{
		_colliders = ModelPhysicsParser::LoadCapsulesFromJSON(_desc.physicsAssetPath);
	}
	_desc.debugLoadedCapsuleCount = static_cast<int32>(_colliders.size());

	LOG_INFO(
		"[PoseFollowChainModifier][Init] tag={} groups={} colliders={} overrideCapsules={} follow={} tipFollow={} damping={} windScale={}",
		String::ToString(_desc.modifierTag),
		_desc.groups.size(),
		_colliders.size(),
		!_desc.overrideCapsules.empty(),
		_desc.poseFollowStrength,
		_desc.tipFollowScale,
		_desc.damping,
		_desc.windScale);

	return S_OK;
}

void PoseFollowChainModifier::BuildChainRecursive(ModelCom* model, int32 currentBoneIndex, int32 parentNodeIndex, BoneChain& outChain)
{
	BoneChainNode node{};
	node.boneIndex = currentBoneIndex;
	node.parentNodeIndex = parentNodeIndex;
	node.poseLocal = model->Get_BoneLocalMatrix(currentBoneIndex);

	if (parentNodeIndex >= 0)
	{
		node.length = std::max(0.01f, node.poseLocal.Translation().Length());
	}

	const int32 thisNodeIndex = static_cast<int32>(outChain.nodes.size());
	outChain.nodes.push_back(node);

	if (parentNodeIndex >= 0)
	{
		outChain.nodes[parentNodeIndex].childNodeIndices.push_back(thisNodeIndex);
	}

	const uint32 numBones = static_cast<uint32>(model->Get_NumBones());
	for (uint32 i = 0; i < numBones; ++i)
	{
		if (model->Get_BoneParentIndex(i) == currentBoneIndex)
		{
			if (std::find(_configuredRootBoneIndices.begin(), _configuredRootBoneIndices.end(), static_cast<int32>(i)) != _configuredRootBoneIndices.end())
			{
				continue;
			}

			BuildChainRecursive(model, static_cast<int32>(i), thisNodeIndex, outChain);
		}
	}
}

void PoseFollowChainModifier::Initialize_Chains(ModelCom* model)
{
	_chains.clear();
	_configuredRootBoneIndices.clear();

	if (!model)
	{
		_chainsInitialized = true;
		return;
	}

	for (const auto& groupDesc : _desc.groups)
	{
		const int32 rootBoneIndex = model->Get_BoneIndex(groupDesc.rootBoneName);
		if (rootBoneIndex != -1
			&& std::find(_configuredRootBoneIndices.begin(), _configuredRootBoneIndices.end(), rootBoneIndex) == _configuredRootBoneIndices.end())
		{
			_configuredRootBoneIndices.push_back(rootBoneIndex);
		}
	}

	for (const auto& groupDesc : _desc.groups)
	{
		const int32 rootBoneIndex = model->Get_BoneIndex(groupDesc.rootBoneName);
		if (rootBoneIndex == -1)
		{
			LOG_WARN("[PoseFollowChainModifier] Missing root bone: {}", groupDesc.rootBoneName);
			continue;
		}

		BoneChain chain{};
		chain.rootBoneName = groupDesc.rootBoneName;
		chain.windMul = groupDesc.windMul;
		chain.motionMul = groupDesc.motionMul;
		chain.followMul = groupDesc.followMul;
		chain.rootOutputMul = groupDesc.rootOutputMul;
		chain.tipOutputMul = groupDesc.tipOutputMul;

		const int32 parentBoneIndex = model->Get_BoneParentIndex(rootBoneIndex);
		if (parentBoneIndex != -1)
		{
			BoneChainNode anchor{};
			anchor.boneIndex = parentBoneIndex;
			anchor.parentNodeIndex = -1;
			anchor.isVirtualAnchor = true;
			chain.nodes.push_back(anchor);
			BuildChainRecursive(model, rootBoneIndex, 0, chain);
		}
		else
		{
			BuildChainRecursive(model, rootBoneIndex, -1, chain);
		}

		if (chain.nodes.size() > 1)
		{
			_chains.push_back(std::move(chain));
		}
	}

	const Matrix ownerWorld = Get_OwnerWorld(_desc);
	Capture_CurrentPose(model, ownerWorld);

	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			node.prevOriginalWorld = node.originalWorld;
			node.position = node.originalWorld.Translation();
			node.prevPosition = node.position;
			node.smoothedLocalDir = node.poseLocalDir;
			node.hasSmoothedLocalDir = node.poseLocalDir.LengthSquared() > 0.0001f;
		}
	}

	LOG_INFO("[PoseFollowChainModifier][BuildChain] tag={} chains={}", String::ToString(_desc.modifierTag), _chains.size());
	_chainsInitialized = true;
}

void PoseFollowChainModifier::Capture_CurrentPose(ModelCom* model, const Matrix& ownerWorld)
{
	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			node.prevOriginalWorld = node.originalWorld;
		}

		for (auto& node : chain.nodes)
		{
			const Matrix* combined = model->Get_BoneMatrixPtr(node.boneIndex);
			node.originalWorld = combined ? (*combined) * ownerWorld : ownerWorld;

			if (!node.isVirtualAnchor)
			{
				node.poseLocal = model->Get_BoneLocalMatrix(node.boneIndex);
			}
		}

		for (auto& node : chain.nodes)
		{
			node.poseLocalDir = Vec3::Zero;
			if (node.parentNodeIndex < 0 || node.isVirtualAnchor)
			{
				continue;
			}

			const auto& parentNode = chain.nodes[node.parentNodeIndex];
			Vec3 poseWorldDir = node.originalWorld.Translation() - parentNode.originalWorld.Translation();
			const float poseLength = poseWorldDir.Length();
			if (poseLength <= 0.0001f)
			{
				continue;
			}

			node.length = std::max(0.01f, poseLength);
			node.poseLocalDir = Get_LocalDirection(parentNode.originalWorld, poseWorldDir);
		}
	}
}

void PoseFollowChainModifier::ResetPose(ModelCom* model)
{
	if (!model)
	{
		return;
	}

	const Matrix ownerWorld = Get_OwnerWorld(_desc);
	Capture_CurrentPose(model, ownerWorld);

	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			node.prevOriginalWorld = node.originalWorld;
			node.position = node.originalWorld.Translation();
			node.prevPosition = node.position;
			node.smoothedLocalDir = node.poseLocalDir;
			node.hasSmoothedLocalDir = node.poseLocalDir.LengthSquared() > 0.0001f;
		}
	}
}

const ClassReflectionInfo* PoseFollowChainModifier::Get_RuntimeReflectionInfo() const
{
	static ClassReflectionInfo info = []()
		{
			ClassReflectionInfo result{};
			result.className = "PoseFollowChainModifierRuntime";
			result.displayName = "Pose Follow Chain Runtime";
			result.category = "Animation";
			result.type = RegisteredType::None;

			AddRuntimeProperty(result.properties, PropertyType::Bool, "enableCollision", "Collision", PoseFollowOffset(&PoseFollowChainModifierDesc::enableCollision));
			AddRuntimeProperty(result.properties, PropertyType::Float, "particleCollisionRadius", "Col Radius", PoseFollowOffset(&PoseFollowChainModifierDesc::particleCollisionRadius), 0.01f, 0.f, 100.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "collisionRadiusPadding", "Col Pad", PoseFollowOffset(&PoseFollowChainModifierDesc::collisionRadiusPadding), 0.01f, 0.f, 100.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "collisionPrimitiveScale", "Col Scale", PoseFollowOffset(&PoseFollowChainModifierDesc::collisionPrimitiveScale), 0.005f, 0.001f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "collisionOutputAlpha", "Col Out", PoseFollowOffset(&PoseFollowChainModifierDesc::collisionOutputAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Bool, "debugDrawCapsules", "Debug Col", PoseFollowOffset(&PoseFollowChainModifierDesc::debugDrawCapsules));
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugLoadedCapsuleCount", "Loaded Col", PoseFollowOffset(&PoseFollowChainModifierDesc::debugLoadedCapsuleCount), 1.f, 0.f, 64.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugActiveCapsuleCount", "Active Col", PoseFollowOffset(&PoseFollowChainModifierDesc::debugActiveCapsuleCount), 1.f, 0.f, 64.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugCollisionSource", "Col Source", PoseFollowOffset(&PoseFollowChainModifierDesc::debugCollisionSource), 1.f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugCollisionHitCount", "Hit Col", PoseFollowOffset(&PoseFollowChainModifierDesc::debugCollisionHitCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugFinalCollisionHitCount", "Final Hit", PoseFollowOffset(&PoseFollowChainModifierDesc::debugFinalCollisionHitCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "debugMinCollisionMargin", "Min Margin", PoseFollowOffset(&PoseFollowChainModifierDesc::debugMinCollisionMargin), 0.01f, -100.f, 100.f);

			AddRuntimeProperty(result.properties, PropertyType::Vec3, "windDirection", "Wind Dir", PoseFollowOffset(&PoseFollowChainModifierDesc::windDirection), 0.01f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "windScale", "Wind", PoseFollowOffset(&PoseFollowChainModifierDesc::windScale), 0.001f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "ownerMotionScale", "Owner Move", PoseFollowOffset(&PoseFollowChainModifierDesc::ownerMotionScale), 0.001f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "animatedMotionScale", "Anim Move", PoseFollowOffset(&PoseFollowChainModifierDesc::animatedMotionScale), 0.005f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "ownerMotionDeadzone", "Owner Dead", PoseFollowOffset(&PoseFollowChainModifierDesc::ownerMotionDeadzone), 0.001f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "ownerRotationDeadzoneDegree", "Rot Dead", PoseFollowOffset(&PoseFollowChainModifierDesc::ownerRotationDeadzoneDegree), 0.05f, 0.f, 180.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "animatedMotionDeadzone", "Anim Dead", PoseFollowOffset(&PoseFollowChainModifierDesc::animatedMotionDeadzone), 0.001f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "maxExternalOffset", "Move Limit", PoseFollowOffset(&PoseFollowChainModifierDesc::maxExternalOffset), 0.001f, 0.f, 100.f);

			AddRuntimeProperty(result.properties, PropertyType::Float, "damping", "Damping", PoseFollowOffset(&PoseFollowChainModifierDesc::damping), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "tipDampingScale", "Tip Damp", PoseFollowOffset(&PoseFollowChainModifierDesc::tipDampingScale), 0.01f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "poseFollowStrength", "Follow", PoseFollowOffset(&PoseFollowChainModifierDesc::poseFollowStrength), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "tipFollowScale", "Tip Follow", PoseFollowOffset(&PoseFollowChainModifierDesc::tipFollowScale), 0.01f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "maxBendAngleDegree", "Max Bend", PoseFollowOffset(&PoseFollowChainModifierDesc::maxBendAngleDegree), 0.25f, 0.f, 180.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "tipBendAngleScale", "Tip Bend", PoseFollowOffset(&PoseFollowChainModifierDesc::tipBendAngleScale), 0.01f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "bendLimitStrength", "Bend Limit", PoseFollowOffset(&PoseFollowChainModifierDesc::bendLimitStrength), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "surfaceConstraintStrength", "Surface", PoseFollowOffset(&PoseFollowChainModifierDesc::surfaceConstraintStrength), 0.005f, 0.f, 1.f);

			AddRuntimeProperty(result.properties, PropertyType::Float, "rootOutputAlpha", "Root Out", PoseFollowOffset(&PoseFollowChainModifierDesc::rootOutputAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "tipOutputAlpha", "Tip Out", PoseFollowOffset(&PoseFollowChainModifierDesc::tipOutputAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "idleOutputAlphaScale", "Idle Out", PoseFollowOffset(&PoseFollowChainModifierDesc::idleOutputAlphaScale), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "outputSmoothAlpha", "Smooth", PoseFollowOffset(&PoseFollowChainModifierDesc::outputSmoothAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "idleOutputSmoothAlpha", "Idle Smooth", PoseFollowOffset(&PoseFollowChainModifierDesc::idleOutputSmoothAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "outputDeadzoneDegree", "Out Dead", PoseFollowOffset(&PoseFollowChainModifierDesc::outputDeadzoneDegree), 0.05f, 0.f, 180.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "idleSettleStrength", "Idle Settle", PoseFollowOffset(&PoseFollowChainModifierDesc::idleSettleStrength), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "sleepVelocityThreshold", "Sleep Vel", PoseFollowOffset(&PoseFollowChainModifierDesc::sleepVelocityThreshold), 0.001f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "sleepPoseDistance", "Sleep Dist", PoseFollowOffset(&PoseFollowChainModifierDesc::sleepPoseDistance), 0.001f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "solverIterations", "Solver Iter", PoseFollowOffset(&PoseFollowChainModifierDesc::solverIterations), 1.f, 1.f, 32.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "targetFrameRate", "Target FPS", PoseFollowOffset(&PoseFollowChainModifierDesc::targetFrameRate), 1.f, 0.f, 240.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "resetDistance", "Reset Dist", PoseFollowOffset(&PoseFollowChainModifierDesc::resetDistance), 0.05f, 0.f, 100.f);

			return result;
		}();

	return &info;
}

void PoseFollowChainModifier::On_RuntimeReflectionChanged(ModelCom* model, const PropertyInfo& property)
{
	UNREFERENCED_PARAMETER(property);

	if (!model || !_chainsInitialized || _chains.empty())
	{
		return;
	}

	model->Rebuild_BoneCombinedMatrices();

	const Matrix ownerWorld = Get_OwnerWorld(_desc);
	const Vec3 currentOwnerPos = ownerWorld.Translation();
	Vec3 currentOwnerForward = Get_WorldDirection(ownerWorld, Vec3::UnitZ);
	if (currentOwnerForward.LengthSquared() <= 0.0001f)
	{
		currentOwnerForward = Vec3::UnitZ;
	}

	const float ownerMotionLength = Vec3::Distance(currentOwnerPos, _prevOwnerPos);
	const float ownerRotationDeltaDegree = GetAngleDegree(_prevOwnerForward, currentOwnerForward);
	const bool isOwnerIdle = ownerMotionLength <= _desc.ownerMotionDeadzone
		&& ownerRotationDeltaDegree <= _desc.ownerRotationDeadzoneDegree;

	Write_BackToModel(model, ownerWorld, isOwnerIdle);
	model->Rebuild_BoneCombinedMatrices();
}

string PoseFollowChainModifier::Get_RuntimeGroupRootBoneName(size_t groupIndex) const
{
	if (groupIndex >= _chains.size())
	{
		return {};
	}

	return _chains[groupIndex].rootBoneName;
}

Vec3 PoseFollowChainModifier::Get_RuntimeGroupMultipliers(size_t groupIndex) const
{
	if (groupIndex >= _chains.size())
	{
		return Vec3::Zero;
	}

	const auto& chain = _chains[groupIndex];
	return Vec3(chain.windMul, chain.motionMul, chain.followMul);
}

void PoseFollowChainModifier::Set_RuntimeGroupMultipliers(size_t groupIndex, const Vec3& multipliers)
{
	if (groupIndex >= _chains.size())
	{
		return;
	}

	auto& chain = _chains[groupIndex];
	chain.windMul = std::max(0.f, multipliers.x);
	chain.motionMul = std::max(0.f, multipliers.y);
	chain.followMul = std::max(0.f, multipliers.z);
}

void PoseFollowChainModifier::Set_RuntimeTestOwnerMotion(bool enabled, const Vec3& delta)
{
	_runtimeTestOwnerMotionEnabled = enabled;
	_runtimeTestOwnerMotionDelta = enabled ? delta : Vec3::Zero;
}

void PoseFollowChainModifier::Build_RuntimeDebugNodes(ModelCom* model, vector<RuntimeDebugNode>& outNodes) const
{
	outNodes.clear();
	if (!model)
	{
		return;
	}

	size_t totalNodeCount = 0;
	for (const auto& chain : _chains)
	{
		totalNodeCount += chain.nodes.size();
	}
	outNodes.reserve(totalNodeCount);

	for (size_t groupIndex = 0; groupIndex < _chains.size(); ++groupIndex)
	{
		const auto& chain = _chains[groupIndex];
		for (size_t nodeIndex = 0; nodeIndex < chain.nodes.size(); ++nodeIndex)
		{
			const auto& node = chain.nodes[nodeIndex];

			RuntimeDebugNode debugNode{};
			debugNode.groupIndex = groupIndex;
			debugNode.nodeIndex = nodeIndex;
			debugNode.parentNodeIndex = node.parentNodeIndex;
			debugNode.boneIndex = node.boneIndex;
			debugNode.boneName = model->Get_BoneName(node.boneIndex);
			debugNode.posePosition = node.originalWorld.Translation();
			const bool isRootNode = node.parentNodeIndex >= 0
				&& chain.nodes[static_cast<size_t>(node.parentNodeIndex)].isVirtualAnchor;
			debugNode.simulatedPosition = (node.isVirtualAnchor || isRootNode)
				? debugNode.posePosition
				: node.position;
			debugNode.writeAlpha = node.debugWriteAlpha;
			debugNode.isVirtualAnchor = node.isVirtualAnchor;
			debugNode.isRootNode = node.debugIsRootNode || isRootNode;
			outNodes.push_back(std::move(debugNode));
		}
	}
}

void PoseFollowChainModifier::Build_RuntimeDebugCapsules(ModelCom* model, vector<RuntimeDebugCapsule>& outCapsules) const
{
	outCapsules.clear();
	if (!model || !_desc.enableCollision)
	{
		return;
	}

	const vector<WorldCapsule> worldCapsules = Update_WorldCapsules(model, Get_OwnerWorld(_desc));
	outCapsules.reserve(worldCapsules.size());
	for (const WorldCapsule& capsule : worldCapsules)
	{
		RuntimeDebugCapsule debugCapsule{};
		debugCapsule.startPoint = capsule.startPoint;
		debugCapsule.endPoint = capsule.endPoint;
		debugCapsule.sourceRadius = capsule.radius;
		debugCapsule.collisionRadius = capsule.radius + _desc.particleCollisionRadius + _desc.collisionRadiusPadding;
		outCapsules.push_back(debugCapsule);
	}
}

void PoseFollowChainModifier::RuntimeRebuild(ModelCom* model)
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

void PoseFollowChainModifier::Solve(ModelCom* model, float timeDelta, const Matrix& ownerWorld, const Vec3& ownerMotionDelta, float movementWeight, bool isOwnerIdle)
{
	UNREFERENCED_PARAMETER(model);

	const float delta = std::clamp(timeDelta, 0.f, 1.f / 30.f);
	const float windTimeScale = _desc.targetFrameRate > 1.f
		? std::clamp(delta * _desc.targetFrameRate, 0.f, 4.f)
		: 1.f;

	vector<WorldCapsule> worldCapsules = _desc.enableCollision ? Update_WorldCapsules(model, ownerWorld) : vector<WorldCapsule>{};
	_desc.debugCollisionHitCount = 0;
	_desc.debugFinalCollisionHitCount = 0;
	_desc.debugMinCollisionMargin = FLT_MAX;

	if (_desc.debugDrawCapsules)
	{
		for (const auto& capsule : worldCapsules)
		{
			DebugCapsuleDesc debugDesc{};
			debugDesc.start = capsule.startPoint;
			debugDesc.end = capsule.endPoint;
			debugDesc.radius = capsule.radius;
			debugDesc.style.color = Color(0.3f, 0.85f, 1.f, 1.f);
			debugDesc.style.depthEnabled = false;
			GAME->Draw_DebugCapsule(debugDesc);
		}
	}

	Vec3 windDir = _desc.windDirection;
	if (windDir.LengthSquared() > 0.0001f)
	{
		windDir.Normalize();
	}

	for (auto& chain : _chains)
	{
		const auto isPinnedNode = [&chain](const BoneChainNode& node)
			{
				if (node.parentNodeIndex < 0 || node.isVirtualAnchor)
				{
					return true;
				}

				return chain.nodes[node.parentNodeIndex].isVirtualAnchor;
			};

		for (size_t i = 0; i < chain.nodes.size(); ++i)
		{
			auto& node = chain.nodes[i];
			node.debugCollisionContact = false;
			if (isPinnedNode(node))
			{
				node.position = node.originalWorld.Translation();
				node.prevPosition = node.position;
				continue;
			}

			const float tipRatio = Get_TipRatio(i, chain.nodes.size());
			const float damping = std::clamp(BlendTip(_desc.damping, _desc.damping * _desc.tipDampingScale, tipRatio), 0.f, 0.98f);
			Vec3 velocity = (node.position - node.prevPosition) * damping;
			if (velocity.LengthSquared() < 0.0000001f)
			{
				velocity = Vec3::Zero;
			}

			const auto& parentNode = chain.nodes[node.parentNodeIndex];
			Vec3 poseWorldDir = Get_WorldDirection(parentNode.originalWorld, node.poseLocalDir);
			if (poseWorldDir.LengthSquared() <= 0.0001f)
			{
				poseWorldDir = node.originalWorld.Translation() - parentNode.originalWorld.Translation();
				if (poseWorldDir.LengthSquared() > 0.0001f)
				{
					poseWorldDir.Normalize();
				}
			}

			Vec3 windTargetOffset = Vec3::Zero;
			if (windDir.LengthSquared() > 0.0001f && poseWorldDir.LengthSquared() > 0.0001f)
			{
				const float parallel = windDir.Dot(poseWorldDir);
				Vec3 lateralWind = windDir - poseWorldDir * parallel;
				if (lateralWind.LengthSquared() > 0.0001f)
				{
					lateralWind.Normalize();
					const float nodeRatio = chain.nodes.size() > 1 ? static_cast<float>(i) / static_cast<float>(chain.nodes.size() - 1) : 0.f;
					const float windWeight = SmoothStep(std::clamp((nodeRatio - 0.18f) / 0.82f, 0.f, 1.f));
					windTargetOffset += lateralWind * (_desc.windScale * chain.windMul * node.length * windWeight);
				}
			}

			const Vec3 animatedDelta = node.originalWorld.Translation() - node.prevOriginalWorld.Translation();
			const float animatedMotionLength = animatedDelta.Length();
			const float animatedMotionDeadzone = (isOwnerIdle && _desc.windScale <= 0.f)
				? std::max(_desc.animatedMotionDeadzone, _desc.sleepPoseDistance)
				: _desc.animatedMotionDeadzone;
			const float nodeRatio = chain.nodes.size() > 1 ? static_cast<float>(i) / static_cast<float>(chain.nodes.size() - 1) : 0.f;
			const float motionWeightByTip = SmoothStep(std::clamp((nodeRatio - 0.08f) / 0.92f, 0.f, 1.f));
			Vec3 externalOffset = Vec3::Zero;
			if (!isOwnerIdle)
			{
				externalOffset -= ownerMotionDelta
					* (_desc.ownerMotionScale * chain.motionMul * motionWeightByTip * movementWeight * windTimeScale);
			}
			if (animatedMotionLength > animatedMotionDeadzone)
			{
				externalOffset -= animatedDelta
					* (_desc.animatedMotionScale * chain.motionMul * motionWeightByTip * movementWeight * windTimeScale);
			}

			const float externalLength = externalOffset.Length();
			const float maxExternalOffset = _desc.maxExternalOffset * movementWeight;
			bool externalClamped = false;
			if (externalLength > maxExternalOffset && externalLength > 0.0001f)
			{
				externalOffset *= maxExternalOffset / externalLength;
				externalClamped = true;
			}

			Vec3 nextPosition = node.position + velocity + externalOffset;
			node.prevPosition = node.position;

			Vec3 poseTarget = node.originalWorld.Translation();
			if (poseWorldDir.LengthSquared() > 0.0001f)
			{
				poseTarget = parentNode.position + poseWorldDir * node.length + windTargetOffset;
				if (_desc.enableCollision)
				{
					poseTarget = Resolve_Collision(node, worldCapsules, poseTarget, false);
				}

				const float followAlpha = std::clamp(
					BlendTip(_desc.poseFollowStrength, _desc.poseFollowStrength * _desc.tipFollowScale, tipRatio) * chain.followMul,
					0.f,
					1.f);
				nextPosition = Vec3::Lerp(nextPosition, poseTarget, followAlpha);
			}

			const bool isNodeIdle = animatedMotionLength <= animatedMotionDeadzone;
			if (isOwnerIdle && isNodeIdle)
			{
				const float settleAlpha = std::clamp(_desc.idleSettleStrength, 0.f, 1.f);
				nextPosition = Vec3::Lerp(nextPosition, poseTarget, settleAlpha);
				node.prevPosition = Vec3::Lerp(node.prevPosition, nextPosition, settleAlpha);

				const bool nearPose = Vec3::Distance(nextPosition, poseTarget) <= _desc.sleepPoseDistance;
				const bool canSnapIdlePose = nearPose
					&& (_desc.windScale <= 0.f || velocity.Length() <= _desc.sleepVelocityThreshold);
				if (canSnapIdlePose)
				{
					nextPosition = poseTarget;
					node.prevPosition = poseTarget;
				}
			}

			Vec3 parentToNode = nextPosition - parentNode.position;
			const float currentLength = parentToNode.Length();
			if (currentLength > 0.0001f)
			{
				nextPosition = parentNode.position + (parentToNode / currentLength) * node.length;
			}

			Vec3 simulatedWorldDir = nextPosition - parentNode.position;
			if (simulatedWorldDir.LengthSquared() > 0.0001f && node.poseLocalDir.LengthSquared() > 0.0001f)
			{
				simulatedWorldDir.Normalize();
				const Vec3 simulatedLocalDir = Get_LocalDirection(parentNode.originalWorld, simulatedWorldDir);
				const float maxBendAngle = BlendTip(_desc.maxBendAngleDegree, _desc.maxBendAngleDegree * _desc.tipBendAngleScale, tipRatio);
				const Vec3 limitedLocalDir = Limit_LocalDirectionByAngle(node.poseLocalDir, simulatedLocalDir, maxBendAngle);
				const Vec3 limitedWorldDir = Get_WorldDirection(parentNode.originalWorld, limitedLocalDir);
				if (limitedWorldDir.LengthSquared() > 0.0001f)
				{
					const float bendAlpha = std::clamp(BlendTip(_desc.bendLimitStrength, _desc.bendLimitStrength * 0.82f, tipRatio), 0.f, 1.f);
					const Vec3 limitedTarget = parentNode.position + limitedWorldDir * node.length;
					nextPosition = Vec3::Lerp(nextPosition, limitedTarget, bendAlpha);
				}
			}

			if (_desc.enableCollision)
			{
				nextPosition = Resolve_Collision(node, worldCapsules, nextPosition, true);
			}

			if (externalClamped && poseWorldDir.LengthSquared() > 0.0001f)
			{
				const float followAlpha = std::clamp(
					BlendTip(_desc.poseFollowStrength, _desc.poseFollowStrength * _desc.tipFollowScale, tipRatio) * chain.followMul,
					0.f,
					1.f);
				const float shapeGuardRadius = std::max(0.0001f, std::max(_desc.maxExternalOffset, maxExternalOffset));
				const float softGuardRadius = shapeGuardRadius * 0.65f;
				Vec3 poseDeviation = nextPosition - poseTarget;
				float poseDeviationLength = poseDeviation.Length();
				if (poseDeviationLength > shapeGuardRadius && poseDeviationLength > 0.0001f)
				{
					const Vec3 guardedPosition = poseTarget + (poseDeviation / poseDeviationLength) * shapeGuardRadius;
					const Vec3 guardCorrection = guardedPosition - nextPosition;
					nextPosition = guardedPosition;
					node.prevPosition += guardCorrection;
					poseDeviation = nextPosition - poseTarget;
					poseDeviationLength = poseDeviation.Length();
				}

				const float overGuardRatio = (shapeGuardRadius > softGuardRadius)
					? std::clamp((poseDeviationLength - softGuardRadius) / (shapeGuardRadius - softGuardRadius), 0.f, 1.f)
					: 1.f;
				const float guardAlpha = SmoothStep(overGuardRatio);
				const float windupBleedAlpha = std::clamp(followAlpha * std::lerp(0.25f, 0.75f, guardAlpha), 0.08f, 0.65f);
				const float windupRecoverAlpha = std::clamp(followAlpha * std::lerp(0.05f, 0.35f, guardAlpha), 0.02f, 0.35f);
				nextPosition = Vec3::Lerp(nextPosition, poseTarget, windupRecoverAlpha * guardAlpha);
				node.prevPosition = Vec3::Lerp(node.prevPosition, nextPosition, windupBleedAlpha);
			}

			if (_desc.enableCollision)
			{
				const Vec3 beforeFinalCollision = nextPosition;
				nextPosition = Resolve_Collision(node, worldCapsules, nextPosition, true);
				if (Vec3::DistanceSquared(beforeFinalCollision, nextPosition) > 0.000001f)
					++_desc.debugFinalCollisionHitCount;
				_desc.debugMinCollisionMargin = std::min(
					_desc.debugMinCollisionMargin,
					Compute_CollisionMargin(worldCapsules, nextPosition));
				node.debugCollisionContact = Compute_CollisionMargin(worldCapsules, nextPosition) <= 0.01f;
			}

			node.position = nextPosition;
		}
	}

	if (_desc.debugMinCollisionMargin == FLT_MAX)
		_desc.debugMinCollisionMargin = 0.f;

	Solve_SurfaceConstraints(worldCapsules);
}

void PoseFollowChainModifier::Solve_SurfaceConstraints(const vector<WorldCapsule>& worldCapsules)
{
	const float strength = std::clamp(_desc.surfaceConstraintStrength, 0.f, 1.f);
	if (strength <= 0.f || _chains.size() < 2)
	{
		return;
	}

	const auto isPinnedNode = [](const BoneChain& chain, const BoneChainNode& node)
		{
			if (node.parentNodeIndex < 0 || node.isVirtualAnchor)
			{
				return true;
			}

			return chain.nodes[node.parentNodeIndex].isVirtualAnchor;
		};

	const auto moveNode = [this, &worldCapsules](BoneChainNode& node, const Vec3& offset)
		{
			node.position += offset;
			node.prevPosition += offset;
			if (_desc.enableCollision)
			{
				const Vec3 corrected = Resolve_Collision(node, worldCapsules, node.position, true);
				const Vec3 correction = corrected - node.position;
				node.position = corrected;
				node.prevPosition += correction;
			}
		};

	for (size_t chainIndex = 0; chainIndex + 1 < _chains.size(); ++chainIndex)
	{
		auto& leftChain = _chains[chainIndex];
		auto& rightChain = _chains[chainIndex + 1];
		const size_t nodeCount = std::min(leftChain.nodes.size(), rightChain.nodes.size());
		for (size_t nodeIndex = 1; nodeIndex < nodeCount; ++nodeIndex)
		{
			auto& leftNode = leftChain.nodes[nodeIndex];
			auto& rightNode = rightChain.nodes[nodeIndex];
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

			const float nodeRatio = nodeCount > 1 ? static_cast<float>(nodeIndex) / static_cast<float>(nodeCount - 1) : 0.f;
			const float tipRatio = SmoothStep(std::clamp((nodeRatio - 0.12f) / 0.88f, 0.f, 1.f));
			const float nodeStrength = strength * BlendTip(1.f, 0.5f, tipRatio);
			const float error = currentDistance - restDistance;
			const Vec3 correction = (currentDelta / currentDistance) * (error * nodeStrength);
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

void PoseFollowChainModifier::Write_BackToModel(ModelCom* model, const Matrix& ownerWorld, bool isOwnerIdle)
{
	if (!model)
	{
		return;
	}

	for (auto& chain : _chains)
	{
		Write_BackChain(model, chain, ownerWorld, isOwnerIdle);
	}
}

void PoseFollowChainModifier::Write_BackChain(ModelCom* model, BoneChain& chain, const Matrix& ownerWorld, bool isOwnerIdle)
{
	const float outputAlpha = std::clamp(_strengthAlpha, 0.f, 1.f);
	const auto isRootNode = [&chain](const BoneChainNode& node)
		{
			return node.parentNodeIndex >= 0 && chain.nodes[node.parentNodeIndex].isVirtualAnchor;
		};

	for (auto& node : chain.nodes)
	{
		node.debugWriteAlpha = 0.f;
		node.debugIsRootNode = isRootNode(node);
	}

	for (size_t i = 0; i < chain.nodes.size(); ++i)
	{
		auto& node = chain.nodes[i];
		if (!isRootNode(node))
		{
			continue;
		}

		if (node.childNodeIndices.empty())
		{
			model->Set_BoneLocalMatrix(node.boneIndex, node.poseLocal);
			continue;
		}

		auto& childNode = chain.nodes[node.childNodeIndices.front()];
		Vec3 simulatedWorldDir = childNode.position - node.position;
		if (simulatedWorldDir.LengthSquared() <= 0.0001f || childNode.poseLocalDir.LengthSquared() <= 0.0001f)
		{
			model->Set_BoneLocalMatrix(node.boneIndex, node.poseLocal);
			continue;
		}

		simulatedWorldDir.Normalize();
		const Vec3 simulatedLocalDir = Get_LocalDirection(node.originalWorld, simulatedWorldDir);
		if (simulatedLocalDir.LengthSquared() <= 0.0001f)
		{
			model->Set_BoneLocalMatrix(node.boneIndex, node.poseLocal);
			continue;
		}

		const Quaternion deltaRotation = Build_RotationDelta(childNode.poseLocalDir, simulatedLocalDir);
		const float writeAlpha = outputAlpha
			* std::clamp(_desc.rootOutputAlpha, 0.f, 1.f)
			* std::clamp(chain.rootOutputMul, 0.f, 1.f);
		node.debugWriteAlpha = writeAlpha;
		if (writeAlpha <= 0.0001f)
		{
			model->Set_BoneLocalMatrix(node.boneIndex, node.poseLocal);
			continue;
		}

		Quaternion blendedDelta = Quaternion::Slerp(Quaternion::Identity, deltaRotation, writeAlpha);
		blendedDelta.Normalize();
		model->Set_BoneLocalMatrix(node.boneIndex, Build_LocalMatrix_RotationOnly(node.poseLocal, blendedDelta));
	}

	model->Rebuild_BoneCombinedMatrices();

	vector<size_t> drivenNodeIndices{};
	drivenNodeIndices.reserve(chain.nodes.size());
	for (size_t i = 0; i < chain.nodes.size(); ++i)
	{
		const auto& node = chain.nodes[i];
		if (node.isVirtualAnchor || node.parentNodeIndex < 0 || isRootNode(node))
		{
			continue;
		}

		drivenNodeIndices.push_back(i);
	}

	for (size_t drivenOrdinal = 0; drivenOrdinal < drivenNodeIndices.size(); ++drivenOrdinal)
	{
		const size_t i = drivenNodeIndices[drivenOrdinal];
		auto& node = chain.nodes[i];
		auto& parentNode = chain.nodes[node.parentNodeIndex];
		const Matrix* parentCombined = model->Get_BoneMatrixPtr(parentNode.boneIndex);
		if (!parentCombined)
		{
			continue;
		}

		const Matrix parentWorld = (*parentCombined) * ownerWorld;
		Vec3 simulatedWorldDir = node.position - parentNode.position;
		if (simulatedWorldDir.LengthSquared() <= 0.0001f || node.poseLocalDir.LengthSquared() <= 0.0001f)
		{
			continue;
		}

		simulatedWorldDir.Normalize();
		const Vec3 simulatedLocalDir = Get_LocalDirection(parentWorld, simulatedWorldDir);
		if (simulatedLocalDir.LengthSquared() <= 0.0001f)
		{
			continue;
		}

		const float rawDrivenRatio = drivenNodeIndices.size() <= 1
			? 1.f
			: static_cast<float>(drivenOrdinal) / static_cast<float>(drivenNodeIndices.size() - 1);
		const float tipRatio = SmoothStep(std::clamp((rawDrivenRatio - 0.18f) / 0.82f, 0.f, 1.f));
		const float animatedMotionDeadzone = (isOwnerIdle && _desc.windScale <= 0.f)
			? std::max(_desc.animatedMotionDeadzone, _desc.sleepPoseDistance)
			: _desc.animatedMotionDeadzone;
		const bool idleAnimatedPose =
			Vec3::Distance(node.originalWorld.Translation(), node.prevOriginalWorld.Translation()) <= animatedMotionDeadzone;
		const bool idleNoWindPose = isOwnerIdle && idleAnimatedPose && _desc.windScale <= 0.f;
		const float outputDeadzoneDegree = _desc.outputDeadzoneDegree;
		if (idleNoWindPose && GetAngleDegree(node.poseLocalDir, simulatedLocalDir) <= outputDeadzoneDegree)
		{
			node.debugWriteAlpha = 0.f;
			node.smoothedLocalDir = node.poseLocalDir;
			node.hasSmoothedLocalDir = true;
			model->Set_BoneLocalMatrix(node.boneIndex, node.poseLocal);
			model->Rebuild_BoneCombinedMatrices();
			continue;
		}

		float writeAlpha = outputAlpha
			* BlendTip(
				_desc.rootOutputAlpha * std::clamp(chain.rootOutputMul, 0.f, 1.f),
				_desc.tipOutputAlpha * std::clamp(chain.tipOutputMul, 0.f, 1.f),
				tipRatio)
			* (idleNoWindPose ? std::clamp(_desc.idleOutputAlphaScale, 0.f, 1.f) : 1.f);
		if (node.debugCollisionContact)
			writeAlpha = std::max(writeAlpha, std::clamp(_desc.collisionOutputAlpha, 0.f, 1.f));
		node.debugWriteAlpha = writeAlpha;
		Vec3 targetLocalDir = Vec3::Lerp(node.poseLocalDir, simulatedLocalDir, writeAlpha);
		if (targetLocalDir.LengthSquared() <= 0.0001f)
		{
			targetLocalDir = node.poseLocalDir;
		}
		else
		{
			targetLocalDir.Normalize();
		}

		if (GetAngleDegree(node.poseLocalDir, targetLocalDir) <= outputDeadzoneDegree)
		{
			targetLocalDir = node.poseLocalDir;
		}

		const float smoothAlpha = std::clamp(
			idleAnimatedPose ? _desc.idleOutputSmoothAlpha : _desc.outputSmoothAlpha,
			0.f,
			1.f);

		if (!node.hasSmoothedLocalDir || node.smoothedLocalDir.LengthSquared() <= 0.0001f)
		{
			node.smoothedLocalDir = targetLocalDir;
			node.hasSmoothedLocalDir = true;
		}
		else
		{
			node.smoothedLocalDir = Vec3::Lerp(node.smoothedLocalDir, targetLocalDir, smoothAlpha);
			if (node.smoothedLocalDir.LengthSquared() <= 0.0001f)
			{
				node.smoothedLocalDir = targetLocalDir;
			}
			else
			{
				node.smoothedLocalDir.Normalize();
			}
		}

		if (GetAngleDegree(node.poseLocalDir, node.smoothedLocalDir) <= outputDeadzoneDegree)
		{
			node.smoothedLocalDir = node.poseLocalDir;
		}

		Quaternion deltaRotation = Build_RotationDelta(node.poseLocalDir, node.smoothedLocalDir);
		deltaRotation.Normalize();

		model->Set_BoneLocalMatrix(node.boneIndex, Build_LocalMatrix_RotationOnly(node.poseLocal, deltaRotation));
		model->Rebuild_BoneCombinedMatrices();
	}
}

vector<PoseFollowChainModifier::WorldCapsule> PoseFollowChainModifier::Update_WorldCapsules(const ModelCom* model, const Matrix& ownerWorld) const
{
	vector<WorldCapsule> worldCapsules{};
	worldCapsules.reserve(_colliders.size());

	const ModelCom* collisionModel = _desc.collisionModel ? _desc.collisionModel : model;
	const Matrix collisionOwnerWorld = _desc.collisionOwnerWorldMatrix ? *_desc.collisionOwnerWorldMatrix : ownerWorld;
	_desc.debugLoadedCapsuleCount = static_cast<int32>(_colliders.size());
	_desc.debugActiveCapsuleCount = 0;
	_desc.debugCollisionSource = _desc.collisionModel ? 1 : 0;

	for (const auto& col : _colliders)
	{
		if (!_desc.includedCollisionBones.empty()
			&& std::find(_desc.includedCollisionBones.begin(), _desc.includedCollisionBones.end(), col.boneName) == _desc.includedCollisionBones.end())
		{
			continue;
		}

		const int32 boneIndex = collisionModel->Get_BoneIndex(col.boneName);
		if (boneIndex == -1)
		{
			continue;
		}

		const Matrix* boneCombined = collisionModel->Get_BoneMatrixPtr(boneIndex);
		if (!boneCombined)
		{
			continue;
		}

		const Matrix boneWorld = (*boneCombined) * collisionOwnerWorld;

		Vec3 localAxis = Vec3::Transform(Vec3::UnitY, col.rotationQuat);
		if (localAxis.LengthSquared() <= 0.0001f)
		{
			localAxis = Vec3::UnitY;
		}
		localAxis.Normalize();

		Vec3 worldScale = Vec3::One;
		Quaternion worldRotation = Quaternion::Identity;
		Vec3 worldTranslation = Vec3::Zero;
		Matrix decomposed = boneWorld;
		decomposed.Decompose(worldScale, worldRotation, worldTranslation);

		Vec3 worldAxis = Vec3::TransformNormal(localAxis, boneWorld);
		if (worldAxis.LengthSquared() <= 0.0001f)
		{
			worldAxis = Vec3::UnitY;
		}
		worldAxis.Normalize();

		const float primitiveScale = std::max(0.001f, _desc.collisionPrimitiveScale);
		const Vec3 worldCenter = Vec3::Transform(col.center * primitiveScale, boneWorld);
		const float radiusScale = std::max(std::max(fabsf(worldScale.x), fabsf(worldScale.y)), fabsf(worldScale.z));
		const float lengthScale = std::max(std::max(fabsf(worldScale.x), fabsf(worldScale.y)), fabsf(worldScale.z));

		WorldCapsule capsule{};
		capsule.radius = col.radius * primitiveScale * radiusScale;
		const float scaledHalfLength = col.halfLength * primitiveScale * lengthScale;
		capsule.startPoint = worldCenter + worldAxis * scaledHalfLength;
		capsule.endPoint = worldCenter - worldAxis * scaledHalfLength;
		worldCapsules.push_back(capsule);
	}

	_desc.debugActiveCapsuleCount = static_cast<int32>(worldCapsules.size());
	return worldCapsules;
}

Vec3 PoseFollowChainModifier::Resolve_Collision(BoneChainNode& node, const vector<WorldCapsule>& capsules, const Vec3& position, bool movePrevPosition)
{
	Vec3 correctedPosition = position;
	for (const auto& capsule : capsules)
	{
		const Vec3 lineVector = capsule.endPoint - capsule.startPoint;
		const float lineLengthSq = lineVector.LengthSquared();
		float t = 0.f;
		if (lineLengthSq > 0.0001f)
		{
			t = (correctedPosition - capsule.startPoint).Dot(lineVector) / lineLengthSq;
			t = std::clamp(t, 0.f, 1.f);
		}

		const Vec3 closestPoint = capsule.startPoint + lineVector * t;
		Vec3 pushNormal = correctedPosition - closestPoint;
		const float minDistance = capsule.radius + _desc.particleCollisionRadius + _desc.collisionRadiusPadding;
		const float distanceSq = pushNormal.LengthSquared();
		if (distanceSq >= minDistance * minDistance)
		{
			continue;
		}

		if (pushNormal.LengthSquared() <= 0.0001f)
		{
			pushNormal = node.originalWorld.Translation() - closestPoint;
		}
		if (pushNormal.LengthSquared() <= 0.0001f)
		{
			pushNormal = Vec3::UnitY;
		}

		const float distance = pushNormal.Length();
		if (distance >= minDistance)
		{
			continue;
		}

		pushNormal /= distance;
		const Vec3 offset = pushNormal * (minDistance - distance);
		correctedPosition += offset;
		++_desc.debugCollisionHitCount;
		if (movePrevPosition)
		{
			node.prevPosition += offset;
		}
	}

	return correctedPosition;
}

float PoseFollowChainModifier::Compute_CollisionMargin(const vector<WorldCapsule>& capsules, const Vec3& position) const
{
	if (capsules.empty())
		return 0.f;

	float minMargin = FLT_MAX;
	for (const auto& capsule : capsules)
	{
		const Vec3 lineVector = capsule.endPoint - capsule.startPoint;
		const float lineLengthSq = lineVector.LengthSquared();
		float t = 0.f;
		if (lineLengthSq > 0.0001f)
		{
			t = (position - capsule.startPoint).Dot(lineVector) / lineLengthSq;
			t = std::clamp(t, 0.f, 1.f);
		}

		const Vec3 closestPoint = capsule.startPoint + lineVector * t;
		const float minDistance = capsule.radius + _desc.particleCollisionRadius + _desc.collisionRadiusPadding;
		const float distance = Vec3::Distance(position, closestPoint);
		minMargin = std::min(minMargin, distance - minDistance);
	}

	return minMargin == FLT_MAX ? 0.f : minMargin;
}

void PoseFollowChainModifier::On_ActiveChanged(ModelCom* model, bool isActive)
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

void PoseFollowChainModifier::Evaluate_Modifier(ModelCom* model, float timeDelta)
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

	if (_chains.empty())
	{
		return;
	}

	const Matrix ownerWorld = Get_OwnerWorld(_desc);
	const Vec3 currentOwnerPos = ownerWorld.Translation();
	Vec3 currentOwnerForward = Get_WorldDirection(ownerWorld, Vec3::UnitZ);
	if (currentOwnerForward.LengthSquared() <= 0.0001f)
	{
		currentOwnerForward = Vec3::UnitZ;
	}

	if (_isFirstUpdate)
	{
		_prevOwnerPos = currentOwnerPos;
		_prevOwnerForward = currentOwnerForward;
		ResetPose(model);
		_isFirstUpdate = false;
		return;
	}

	Vec3 ownerMotionDelta = currentOwnerPos - _prevOwnerPos;
	if (_runtimeTestOwnerMotionEnabled)
	{
		ownerMotionDelta += _runtimeTestOwnerMotionDelta;
	}
	float ownerMotionLength = ownerMotionDelta.Length();
	const float ownerRotationDeltaDegree = GetAngleDegree(_prevOwnerForward, currentOwnerForward);
	if (ownerMotionLength > _desc.resetDistance)
	{
		ResetPose(model);
		ownerMotionDelta = Vec3::Zero;
		ownerMotionLength = 0.f;
		_prevOwnerForward = currentOwnerForward;
	}
	const bool isOwnerIdle = ownerMotionLength <= _desc.ownerMotionDeadzone
		&& ownerRotationDeltaDegree <= _desc.ownerRotationDeadzoneDegree;
	_prevOwnerPos = currentOwnerPos;
	_prevOwnerForward = currentOwnerForward;

	Capture_CurrentPose(model, ownerWorld);

	uint32 solverIterations = std::max<uint32>(_desc.solverIterations, 1);
	if (_desc.targetFrameRate > 1.f)
	{
		const float maxSubstepDelta = 1.f / _desc.targetFrameRate;
		const uint32 targetIterations = static_cast<uint32>(std::ceil(std::max(timeDelta, 0.f) / maxSubstepDelta));
		solverIterations = std::max(solverIterations, std::max<uint32>(targetIterations, 1));
	}

	const float solverTimeDelta = timeDelta / static_cast<float>(solverIterations);
	const float movementWeight = 1.f / static_cast<float>(solverIterations);
	for (uint32 i = 0; i < solverIterations; ++i)
	{
		Solve(model, solverTimeDelta, ownerWorld, ownerMotionDelta, movementWeight, isOwnerIdle);
	}

	Write_BackToModel(model, ownerWorld, isOwnerIdle);
	model->Rebuild_BoneCombinedMatrices();
}

Shared<PoseFollowChainModifier> PoseFollowChainModifier::Create(const PoseFollowChainModifierDesc* desc)
{
	Shared<PoseFollowChainModifier> instance = make_shared<PoseFollowChainModifier>();
	if (FAILED(instance->Initialize(const_cast<PoseFollowChainModifierDesc*>(desc))))
	{
		assert(false && "Failed to Create PoseFollowChainModifier");
		return nullptr;
	}

	return instance;
}

void PoseFollowChainModifier::Free()
{
	AnimationModifier::Free();
}

NS_END
