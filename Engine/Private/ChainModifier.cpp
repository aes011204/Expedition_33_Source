#include "ChainModifier.h"
#include "ModelCom.h"
#include "Bone.h"
#include "GameObject.h"
#include "Helper_Math.h"
#include "ModelPhysicsParser.h"
#include "TransformCom.h"
#include "GameInstance.h"
#include "Engine_Macro.h"
#include <algorithm>

NS_BEGIN(Engine)

namespace
{
	Quaternion BuildRootOffsetQuaternion(const Vec3 &rotationOffsetDegrees)
	{
		return Quaternion::CreateFromYawPitchRoll(
			XMConvertToRadians(rotationOffsetDegrees.y),
			XMConvertToRadians(rotationOffsetDegrees.x),
			XMConvertToRadians(rotationOffsetDegrees.z));
	}

	bool IsCoatSurfaceChain(const string &rootBoneName)
	{
		return rootBoneName.find("Coat_") != string::npos
			&& rootBoneName.find("Collar") == string::npos;
	}

	float GetWindNodeWeight(const ChainModifier::BoneChain &chain, size_t nodeIndex, size_t nodeCount)
	{
		if (nodeCount <= 1)
		{
			return 0.f;
		}

		const float ratio = static_cast<float>(nodeIndex) / static_cast<float>(nodeCount - 1);
		const bool isCoatChain = IsCoatSurfaceChain(chain.rootBoneName);
		const float startRatio = isCoatChain ? 0.72f : 0.4f;
		const float tipRatio = Math::NormalizeSegment(ratio, startRatio, 1.f);
		const float weightScale = isCoatChain ? 0.08f : 0.65f;
		return Math::GetSmoothStepRatio(tipRatio) * weightScale;
	}

	float GetChainTipRatio(size_t nodeIndex, size_t nodeCount)
	{
		if (nodeCount <= 1)
		{
			return 0.f;
		}

		const float ratio = static_cast<float>(nodeIndex) / static_cast<float>(nodeCount - 1);
		return Math::GetSmoothStepRatio(std::clamp(ratio, 0.f, 1.f));
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

		const float nodeWeight = GetWindNodeWeight(chain, nodeIndex, chain.nodes.size());
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
		return lateralWind * (desc.runtime.windScale * node.length * nodeWeight * lateralAmount);
	}
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

	if (!_desc.collision.physicsAssetPath.empty())
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

	if (currentDepth >= maxDepth)
	{
		return;
	}

	const uint32 numBones = static_cast<uint32>(model->Get_NumBones());
	for (uint32 i = 0; i < numBones; ++i)
	{
		if (model->Get_BoneParentIndex(i) == currentBoneIndex)
		{
			BuildChainRecursive(model, static_cast<int32>(i), thisNodeIndex, outChain, currentDepth + 1, maxDepth);
		}
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
		if (_desc.shape.pinRoot && chain.nodes[node.parentNodeIndex].isVirtualAnchor
			&& _desc.shape.rootRotationOffsetDegrees.LengthSquared() > 0.0001f)
		{
			bindLocal = Build_LocalMatrix_RotationOnly(node.bindLocal, BuildRootOffsetQuaternion(_desc.shape.rootRotationOffsetDegrees));
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

void ChainModifier::Initialize_Chains(ModelCom *model)
{
	_chains.clear();
	Matrix ownerWorld = _desc.runtime.ownerTransform ? _desc.runtime.ownerTransform->Get_WorldMatrix() : Matrix::Identity;

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
			_chains.push_back(std::move(newChain));
		}
	}

	_chainsInitialized = true;
}

void ChainModifier::ResetPose(ModelCom *model)
{
	Matrix ownerWorld = _desc.runtime.ownerTransform ? _desc.runtime.ownerTransform->Get_WorldMatrix() : Matrix::Identity;

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

void ChainModifier::Capture_CurrentPose(ModelCom *model, const Matrix &ownerWorld)
{
	for (auto &chain : _chains)
	{
		for (auto &node : chain.nodes)
		{
			node.prevOriginalWorld = node.originalWorld;
			node.prevOriginalPosition = node.originalWorld.Translation();
		}

		if (_desc.blendMode == ModifierBlendMode::ModifyAddictive || _desc.runtime.useAnimatedPoseAsRest)
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

		node.poseLocal = node.bindLocal;
		const bool isRootNode = _desc.shape.pinRoot
			&& node.parentNodeIndex >= 0
			&& chain.nodes[node.parentNodeIndex].isVirtualAnchor;
		if (isRootNode && _desc.shape.rootRotationOffsetDegrees.LengthSquared() > 0.0001f)
		{
			node.poseLocal = Build_LocalMatrix_RotationOnly(node.bindLocal, BuildRootOffsetQuaternion(_desc.shape.rootRotationOffsetDegrees));
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
		const Matrix *combinedMat = model->Get_BoneMatrixPtr(node.boneIndex);
		node.poseLocal = model->Get_BoneLocalMatrix(node.boneIndex);
		node.originalWorld = combinedMat ? (*combinedMat) * ownerWorld : ownerWorld;
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

void ChainModifier::Solve_Verlet(ModelCom *model, float timeDelta, const Matrix &ownerWorld, const Vec3 &ownerMotionDelta, float movementWeight)
{
	const float delta = std::clamp(timeDelta, 0.f, 1.f / 30.f);
	vector<WorldCapsule> worldCapsules = _desc.collision.enableCollision ? Update_WorldCapsule(model, &ownerWorld) : vector<WorldCapsule>{};
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
		Vec3 curGravity = _desc.motion.gravity * chain.gravityMul;
		Vec3 chainRootMotionDelta = Vec3::Zero;
		for (const auto &candidateNode : chain.nodes)
		{
			if (!candidateNode.isVirtualAnchor)
			{
				continue;
			}

			chainRootMotionDelta = candidateNode.originalWorld.Translation() - candidateNode.prevOriginalPosition;
			break;
		}

		if (chainRootMotionDelta.LengthSquared() <= 0.0001f)
		{
			for (const auto &candidateNode : chain.nodes)
			{
				if (candidateNode.isVirtualAnchor)
				{
					continue;
				}

				chainRootMotionDelta = candidateNode.originalWorld.Translation() - candidateNode.prevOriginalPosition;
				break;
			}
		}

		if (_desc.runtime.useChainRootMotionInertia && chainRootMotionDelta.LengthSquared() > 0.0001f)
		{
			const Matrix ownerWorldInv = ownerWorld.Invert();
			Vec3 localChainRootMotionDelta = Vec3::TransformNormal(chainRootMotionDelta, ownerWorldInv);
			localChainRootMotionDelta.y = 0.f;

			float planarMotion = localChainRootMotionDelta.Length();
			if (planarMotion <= 0.12f)
			{
				chainRootMotionDelta = Vec3::Zero;
			}
			else
			{
				const float filteredPlanarMotion = std::min(planarMotion - 0.12f, 0.45f);
				if (planarMotion > 0.0001f)
				{
					localChainRootMotionDelta *= filteredPlanarMotion / planarMotion;
				}

				chainRootMotionDelta = Vec3::TransformNormal(localChainRootMotionDelta, ownerWorld);
			}
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

			if (_desc.runtime.useAnimatedPoseAsRest && _desc.runtime.transportParticlesWithAnimatedPose)
			{
				const auto &parentNode = chain.nodes[node.parentNodeIndex];
				const Vec3 parentMotionDelta = parentNode.originalWorld.Translation() - parentNode.prevOriginalWorld.Translation();
				if (parentMotionDelta.Length() > _desc.runtime.animatedPoseTransportDeadzone)
				{
					const Matrix prevParentWorldInv = parentNode.prevOriginalWorld.Invert();

					const Vec3 localPosition = Vec3::Transform(node.position, prevParentWorldInv);
					const Vec3 localPrevPosition = Vec3::Transform(node.prevPosition, prevParentWorldInv);

					const Vec3 transportedPosition = Vec3::Transform(localPosition, parentNode.originalWorld);
					const Vec3 transportedPrevPosition = Vec3::Transform(localPrevPosition, parentNode.originalWorld);

					const float transportAlpha = std::clamp(_desc.runtime.animatedPoseTransportAlpha, 0.f, 1.f);
					node.position = Vec3::Lerp(node.position, transportedPosition, transportAlpha);
					node.prevPosition = Vec3::Lerp(node.prevPosition, transportedPrevPosition, transportAlpha);
				}
			}

			const float tipRatio = GetChainTipRatio(i, chain.nodes.size());
			const float nodeShapeStiffness = std::clamp(
				BlendTipScale(shapeStiffness, _desc.shape.tipStiffnessScale, tipRatio),
				0.f,
				1.f);
			const float nodeMovementInertiaScale = BlendTipScale(
				_desc.motion.movementInertiaScale,
				_desc.motion.tipMovementInertiaScale,
				tipRatio);

			Vec3 movementOffset = Vec3::Zero;
			if (nodeMovementInertiaScale > 0.f)
			{
				if (_desc.runtime.useChainRootMotionInertia)
				{
					movementOffset = -chainRootMotionDelta * nodeMovementInertiaScale * chain.movementInertiaMul * movementWeight;
				}
				else if (_desc.runtime.useOwnerMotionInertia)
				{
					movementOffset = -ownerMotionDelta * nodeMovementInertiaScale * chain.movementInertiaMul * movementWeight;
				}
				else
				{
					movementOffset = -(node.originalWorld.Translation() - node.prevOriginalPosition) * nodeMovementInertiaScale * chain.movementInertiaMul * movementWeight;
				}
			}

			const float movementOffsetLength = movementOffset.Length();
			const float maxMovementOffset = _desc.motion.maxMovementOffset * movementWeight;
			if (movementOffsetLength > maxMovementOffset && movementOffsetLength > 0.0001f)
			{
				movementOffset /= movementOffsetLength;
				movementOffset *= maxMovementOffset;
			}

			Vec3 nextPosition = Math::SolveVerlet(node.position, node.prevPosition, curGravity, curDamping, delta);
			nextPosition += movementOffset;

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
				const Vec3 poseWorldDir = Get_WorldDirection(parentBasisWorld, node.poseLocalDir);
				if (poseWorldDir.LengthSquared() > 0.0001f)
				{
					const Vec3 windOffset = ComputeWindOffset(_desc, chain, node, i);
					const Vec3 restTarget = chain.nodes[node.parentNodeIndex].position + poseWorldDir * node.length + windOffset;
					node.position = Vec3::Lerp(node.position, restTarget, restoreAlpha);
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
				node.position = parentNode.position + (parentToNode / currentDist) * node.length;
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
			if (node.poseLocalDir.LengthSquared() <= 0.0001f || simulatedDir.LengthSquared() <= 0.0001f)
			{
				continue;
			}

			const float tipRatio = GetChainTipRatio(i, chain.nodes.size());
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
				node.poseLocalDir,
				simulatedLocalDir,
				nodeMaxBendAngleDegree);
			const Vec3 limitedWorldDir = Get_WorldDirection(parentNode.originalWorld, limitedLocalDir);
			if (limitedWorldDir.LengthSquared() > 0.0001f)
			{
				const Vec3 limitedTarget = parentNode.position + limitedWorldDir * node.length;
				node.position = Vec3::Lerp(node.position, limitedTarget, nodeShapeStiffness);
			}

			if (_desc.collision.enableCollision)
			{
				node.position = Compute_WorldCapsule(node, worldCapsules, node.position);
			}
		}
	}

	Solve_SurfaceConstraints(worldCapsules);
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
		if (!IsCoatSurfaceChain(leftChain.rootBoneName) || !IsCoatSurfaceChain(rightChain.rootBoneName))
		{
			continue;
		}

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
	const auto isRootNode = [this, &chain](const BoneChainNode &node)
		{
			return _desc.shape.pinRoot && node.parentNodeIndex >= 0 && chain.nodes[node.parentNodeIndex].isVirtualAnchor;
		};

	for (size_t i = 0; i < chain.nodes.size(); ++i)
	{
		auto &node = chain.nodes[i];
		if (!isRootNode(node))
		{
			continue;
		}

		Matrix baseLocal = node.poseLocal;
		if (node.childNodeIndices.empty())
		{
			model->Set_BoneLocalMatrix(node.boneIndex, baseLocal);
			continue;
		}

		auto &firstChild = chain.nodes[node.childNodeIndices.front()];
		Vec3 simulatedWorldDir = firstChild.position - node.position;
		if (simulatedWorldDir.LengthSquared() <= 0.0001f || firstChild.poseLocalDir.LengthSquared() <= 0.0001f)
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

		const float tipRatio = GetChainTipRatio(static_cast<size_t>(node.childNodeIndices.front()), chain.nodes.size());
		const float nodeShapeStiffness = std::clamp(
			BlendTipScale(shapeStiffness, _desc.shape.tipStiffnessScale, tipRatio),
			0.f,
			1.f);
		const float nodeMaxBendAngleDegree = BlendTipScale(
			_desc.shape.maxBendAngleDegree,
			_desc.shape.tipMaxBendAngleScale,
			tipRatio) * chain.maxBendAngleMul;

		const Vec3 limitedLocalDir = Limit_LocalDirectionByAngle(firstChild.poseLocalDir, simulatedLocalDir, nodeMaxBendAngleDegree);
		Vec3 blendedLocalDir = Vec3::Lerp(simulatedLocalDir, limitedLocalDir, nodeShapeStiffness);
		if (blendedLocalDir.LengthSquared() > 0.0001f)
		{
			blendedLocalDir.Normalize();
		}
		Quaternion deltaRotation = Build_RotationDelta(firstChild.poseLocalDir, blendedLocalDir);
		deltaRotation = Quaternion::Slerp(Quaternion::Identity, deltaRotation, outputAlpha);
		deltaRotation.Normalize();
		model->Set_BoneLocalMatrix(node.boneIndex, Build_LocalMatrix_RotationOnly(baseLocal, deltaRotation));
	}
	model->Rebuild_BoneCombinedMatrices();

	for (size_t i = 0; i < chain.nodes.size(); ++i)
	{
		auto &node = chain.nodes[i];
		if (node.isVirtualAnchor || node.parentNodeIndex < 0 || isRootNode(node))
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
		if (node.poseLocalDir.LengthSquared() <= 0.0001f || simulatedLocalDir.LengthSquared() <= 0.0001f)
		{
			continue;
		}

		const float tipRatio = GetChainTipRatio(i, chain.nodes.size());
		const float nodeShapeStiffness = std::clamp(
			BlendTipScale(shapeStiffness, _desc.shape.tipStiffnessScale, tipRatio),
			0.f,
			1.f);
		const float nodeMaxBendAngleDegree = BlendTipScale(
			_desc.shape.maxBendAngleDegree,
			_desc.shape.tipMaxBendAngleScale,
			tipRatio) * chain.maxBendAngleMul;

		const Vec3 limitedLocalDir = Limit_LocalDirectionByAngle(node.poseLocalDir, simulatedLocalDir, nodeMaxBendAngleDegree);
		Vec3 blendedLocalDir = Vec3::Lerp(simulatedLocalDir, limitedLocalDir, nodeShapeStiffness);
		if (blendedLocalDir.LengthSquared() > 0.0001f)
		{
			blendedLocalDir.Normalize();
		}
		Quaternion deltaRotation = Build_RotationDelta(node.poseLocalDir, blendedLocalDir);
		deltaRotation = Quaternion::Slerp(Quaternion::Identity, deltaRotation, outputAlpha);
		deltaRotation.Normalize();
		model->Set_BoneLocalMatrix(node.boneIndex, Build_LocalMatrix_RotationOnly(node.poseLocal, deltaRotation));
		model->Rebuild_BoneCombinedMatrices();
	}
}

vector<ChainModifier::WorldCapsule> ChainModifier::Update_WorldCapsule(const ModelCom *model, const Matrix *ownerWorld)
{
	vector<WorldCapsule> worldCapsules;
	worldCapsules.reserve(_colliders.size());

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
		localAxis.Normalize();

		Vec3 worldCenter = Vec3::Transform(col.center, boneWorld);
		Vec3 worldAxis = Vec3::TransformNormal(localAxis, boneWorld);

		Vec3 worldScale = Vec3::One;
		Quaternion worldRotation = Quaternion::Identity;
		Vec3 worldTranslation = Vec3::Zero;
		Matrix decomposedBoneWorld = boneWorld;
		decomposedBoneWorld.Decompose(worldScale, worldRotation, worldTranslation);

		worldAxis.Normalize();

		WorldCapsule capsule{};
		capsule.radius = col.radius * std::max(fabsf(worldScale.x), fabsf(worldScale.z));
		float scaledHalfLength = col.halfLength * fabsf(worldScale.y);

		capsule.startPoint = worldCenter + worldAxis * scaledHalfLength;
		capsule.endPoint = worldCenter - worldAxis * scaledHalfLength;
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

		const float minCollisionDist = wc.radius + _desc.collision.collisionRadiusPadding;
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

	if (_desc.modifierTag == L"ClothPhysics_Coat")
	{
		LOG_INFO_ONCE(
			"[ChainModifier][CoatEval] tag={} active={} strengthAlpha={} chainCount={} windScale={} movementInertia={} tipMovementInertia={} animatedRest={} transportAlpha={}",
			String::ToString(_desc.modifierTag),
			_isActive,
			_strengthAlpha,
			_chains.size(),
			_desc.runtime.windScale,
			_desc.motion.movementInertiaScale,
			_desc.motion.tipMovementInertiaScale,
			_desc.runtime.useAnimatedPoseAsRest,
			_desc.runtime.animatedPoseTransportAlpha);
	}

	if (!_chainsInitialized)
	{
		Initialize_Chains(model);
	}

	Matrix ownerWorld = _desc.runtime.ownerTransform ? _desc.runtime.ownerTransform->Get_WorldMatrix() : Matrix::Identity;
	Vec3 currentOwnerPos = ownerWorld.Translation();

	if (_isFirstUpdate)
	{
		_prevOwnerPos = currentOwnerPos;
		ResetPose(model);
		_isFirstUpdate = false;
		return;
	}

	const float ownerDistance = Vec3::Distance(_prevOwnerPos, currentOwnerPos);
	Vec3 ownerMotionDelta = currentOwnerPos - _prevOwnerPos;
	if (ownerDistance > _desc.runtime.resetDistance)
	{
		ResetPose(model);
		ownerMotionDelta = Vec3::Zero;
	}

	_prevOwnerPos = currentOwnerPos;

	Capture_CurrentPose(model, ownerWorld);

	const uint32 solverIterations = std::max<uint32>(_desc.motion.solverIterations, 1);
	const float solverTimeDelta = timeDelta / static_cast<float>(solverIterations);
	const float movementWeight = 1.f / static_cast<float>(solverIterations);

	for (uint32 i = 0; i < solverIterations; ++i)
	{
		Solve_Verlet(model, solverTimeDelta, ownerWorld, ownerMotionDelta, movementWeight);
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
