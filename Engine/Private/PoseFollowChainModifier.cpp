#include "PoseFollowChainModifier.h"
#include "ModelCom.h"
#include "ModelPhysicsParser.h"
#include "GameInstance.h"
#include "Engine_Macro.h"
#include <algorithm>
#include <cmath>

NS_BEGIN(Engine)

namespace
{
	float SmoothStep(float value)
	{
		value = std::clamp(value, 0.f, 1.f);
		return value * value * (3.f - 2.f * value);
	}

	float BlendTip(float rootValue, float tipValue, float tipRatio)
	{
		return std::lerp(rootValue, tipValue, tipRatio);
	}

	bool IsCoatSurfaceChain(const string& rootBoneName)
	{
		return rootBoneName.find("Coat_") != string::npos
			&& rootBoneName.find("Collar") == string::npos;
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

void PoseFollowChainModifier::BuildChainRecursive(ModelCom* model, int32 currentBoneIndex, int32 parentNodeIndex, BoneChain& outChain, uint32 currentDepth, uint32 maxDepth)
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

void PoseFollowChainModifier::Initialize_Chains(ModelCom* model)
{
	_chains.clear();

	if (!model)
	{
		_chainsInitialized = true;
		return;
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

		const int32 parentBoneIndex = model->Get_BoneParentIndex(rootBoneIndex);
		if (parentBoneIndex != -1)
		{
			BoneChainNode anchor{};
			anchor.boneIndex = parentBoneIndex;
			anchor.parentNodeIndex = -1;
			anchor.isVirtualAnchor = true;
			chain.nodes.push_back(anchor);
			BuildChainRecursive(model, rootBoneIndex, 0, chain, 0, groupDesc.maxDepth);
		}
		else
		{
			BuildChainRecursive(model, rootBoneIndex, -1, chain, 0, groupDesc.maxDepth);
		}

		if (chain.nodes.size() > 1)
		{
			_chains.push_back(std::move(chain));
		}
	}

	const Matrix ownerWorld = _desc.ownerTransform ? _desc.ownerTransform->Get_WorldMatrix() : Matrix::Identity;
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

	const Matrix ownerWorld = _desc.ownerTransform ? _desc.ownerTransform->Get_WorldMatrix() : Matrix::Identity;
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

void PoseFollowChainModifier::Solve(ModelCom* model, float timeDelta, const Matrix& ownerWorld, const Vec3& ownerMotionDelta, float movementWeight, bool isOwnerIdle)
{
	UNREFERENCED_PARAMETER(model);

	const float delta = std::clamp(timeDelta, 0.f, 1.f / 30.f);
	const float windTimeScale = _desc.targetFrameRate > 1.f
		? std::clamp(delta * _desc.targetFrameRate, 0.f, 4.f)
		: 1.f;

	vector<WorldCapsule> worldCapsules = _desc.enableCollision ? Update_WorldCapsules(model, ownerWorld) : vector<WorldCapsule>{};

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
					const float windWeight = SmoothStep(std::clamp((Get_NodeRatio(i, chain.nodes.size()) - 0.18f) / 0.82f, 0.f, 1.f));
					windTargetOffset += lateralWind * (_desc.windScale * chain.windMul * node.length * windWeight);
				}
			}

			const Vec3 animatedDelta = node.originalWorld.Translation() - node.prevOriginalWorld.Translation();
			const float animatedMotionLength = animatedDelta.Length();
			const float animatedMotionDeadzone = (isOwnerIdle && _desc.windScale <= 0.f)
				? std::max(_desc.animatedMotionDeadzone, _desc.sleepPoseDistance)
				: _desc.animatedMotionDeadzone;
			const float motionWeightByTip = SmoothStep(std::clamp((Get_NodeRatio(i, chain.nodes.size()) - 0.08f) / 0.92f, 0.f, 1.f));
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
			if (externalLength > maxExternalOffset && externalLength > 0.0001f)
			{
				externalOffset *= maxExternalOffset / externalLength;
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

				if (velocity.Length() <= _desc.sleepVelocityThreshold
					&& Vec3::Distance(nextPosition, poseTarget) <= _desc.sleepPoseDistance)
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

			node.position = nextPosition;
		}
	}

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
		if (!IsCoatSurfaceChain(leftChain.rootBoneName) || !IsCoatSurfaceChain(rightChain.rootBoneName))
		{
			continue;
		}

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

			const float tipRatio = SmoothStep(std::clamp((Get_NodeRatio(nodeIndex, nodeCount) - 0.12f) / 0.88f, 0.f, 1.f));
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
		const float writeAlpha = outputAlpha * std::clamp(_desc.rootOutputAlpha, 0.f, 1.f);
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
		const float writeAlpha = outputAlpha
			* BlendTip(_desc.rootOutputAlpha, _desc.tipOutputAlpha, tipRatio)
			* (idleNoWindPose ? std::clamp(_desc.idleOutputAlphaScale, 0.f, 1.f) : 1.f);
		Vec3 targetLocalDir = Vec3::Lerp(node.poseLocalDir, simulatedLocalDir, writeAlpha);
		if (targetLocalDir.LengthSquared() <= 0.0001f)
		{
			targetLocalDir = node.poseLocalDir;
		}
		else
		{
			targetLocalDir.Normalize();
		}

		const float outputDeadzoneDegree = idleNoWindPose
			? _desc.outputDeadzoneDegree * std::clamp(_desc.idleOutputAlphaScale, 0.f, 1.f)
			: _desc.outputDeadzoneDegree;
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

	for (const auto& col : _colliders)
	{
		if (!_desc.includedCollisionBones.empty()
			&& std::find(_desc.includedCollisionBones.begin(), _desc.includedCollisionBones.end(), col.boneName) == _desc.includedCollisionBones.end())
		{
			continue;
		}

		const int32 boneIndex = model->Get_BoneIndex(col.boneName);
		if (boneIndex == -1)
		{
			continue;
		}

		const Matrix* boneCombined = model->Get_BoneMatrixPtr(boneIndex);
		if (!boneCombined)
		{
			continue;
		}

		const Matrix boneWorld = (*boneCombined) * ownerWorld;

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

		const Vec3 worldCenter = Vec3::Transform(col.center, boneWorld);
		const float radiusScale = std::max(std::max(fabsf(worldScale.x), fabsf(worldScale.y)), fabsf(worldScale.z));
		const float lengthScale = std::max(std::max(fabsf(worldScale.x), fabsf(worldScale.y)), fabsf(worldScale.z));

		WorldCapsule capsule{};
		capsule.radius = col.radius * radiusScale;
		const float scaledHalfLength = col.halfLength * lengthScale;
		capsule.startPoint = worldCenter + worldAxis * scaledHalfLength;
		capsule.endPoint = worldCenter - worldAxis * scaledHalfLength;
		worldCapsules.push_back(capsule);
	}

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
		if (movePrevPosition)
		{
			node.prevPosition += offset;
		}
	}

	return correctedPosition;
}

float PoseFollowChainModifier::Get_NodeRatio(size_t nodeIndex, size_t nodeCount)
{
	if (nodeCount <= 1)
	{
		return 0.f;
	}

	return std::clamp(static_cast<float>(nodeIndex) / static_cast<float>(nodeCount - 1), 0.f, 1.f);
}

float PoseFollowChainModifier::Get_TipRatio(size_t nodeIndex, size_t nodeCount)
{
	return SmoothStep(Get_NodeRatio(nodeIndex, nodeCount));
}

Vec3 PoseFollowChainModifier::Get_LocalDirection(const Matrix& parentWorld, const Vec3& worldDirection)
{
	Vec3 localDirection = Vec3::TransformNormal(worldDirection, parentWorld.Invert());
	if (localDirection.LengthSquared() > 0.0001f)
	{
		localDirection.Normalize();
	}

	return localDirection;
}

Vec3 PoseFollowChainModifier::Get_WorldDirection(const Matrix& parentWorld, const Vec3& localDirection)
{
	Vec3 worldDirection = Vec3::TransformNormal(localDirection, parentWorld);
	if (worldDirection.LengthSquared() > 0.0001f)
	{
		worldDirection.Normalize();
	}

	return worldDirection;
}

Vec3 PoseFollowChainModifier::Limit_LocalDirectionByAngle(const Vec3& poseLocalDirection, const Vec3& simulatedLocalDirection, float maxAngleDegree)
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

Quaternion PoseFollowChainModifier::Build_RotationDelta(const Vec3& fromDirection, const Vec3& toDirection)
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

Matrix PoseFollowChainModifier::Build_LocalMatrix_RotationOnly(const Matrix& baseLocal, const Quaternion& deltaRotation)
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

	if (!_chainsInitialized)
	{
		Initialize_Chains(model);
	}

	if (_chains.empty())
	{
		return;
	}

	const Matrix ownerWorld = _desc.ownerTransform ? _desc.ownerTransform->Get_WorldMatrix() : Matrix::Identity;
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
