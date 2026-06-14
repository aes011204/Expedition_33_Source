#include "ChainModifier.h"
#include "ModelCom.h"
#include "Bone.h"
#include "GameObject.h"
#include "Helper_Math.h"
#include "ModelPhysicsParser.h"
#include "TransformCom.h"

NS_BEGIN(Engine)

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

	if (!_desc.physicsAssetPath.empty())
	{
		_colliders = ModelPhysicsParser::LoadCapsulesFromJSON(_desc.physicsAssetPath);
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
		else
		{
			node.bindLocalDir = localTranslation / node.length;
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

void ChainModifier::Initialize_Chains(ModelCom *model)
{
	_chains.clear();

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
			_chains.push_back(std::move(newChain));
		}
	}

	_chainsInitialized = true;
}

void ChainModifier::ResetPose(ModelCom *model)
{
	Matrix ownerWorld = _desc.ownerTransform ? _desc.ownerTransform->Get_WorldMatrix() : Matrix::Identity;

	Capture_CurrentPose(model, ownerWorld);

	for (auto &chain : _chains)
	{
		for (auto &node : chain.nodes)
		{
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
		for (size_t i = 0; i < chain.nodes.size(); ++i)
		{
			auto &node = chain.nodes[i];
			node.prevOriginalPosition = node.originalWorld.Translation();

			if (_desc.blendMode == ModifierBlendMode::ModifyReplace && !node.isVirtualAnchor)
			{
				if (node.parentNodeIndex >= 0)
				{
					const Matrix &parentWorld = chain.nodes[node.parentNodeIndex].originalWorld;
					node.originalWorld = node.bindLocal * parentWorld;
					continue;
				}

				node.originalWorld = node.bindLocal * ownerWorld;
				continue;
			}

			const Matrix *combinedMat = model->Get_BoneMatrixPtr(node.boneIndex);
			if (combinedMat)
			{
				node.originalWorld = (*combinedMat) * ownerWorld;
			}
			else
			{
				node.originalWorld = ownerWorld;
			}
		}
	}
}

void ChainModifier::Solve_Verlet(ModelCom *model, float dt, const Matrix &ownerWorld, float movementWeight)
{
	const float timeDelta = std::clamp(dt, 0.f, 1.f / 30.f);
	vector<WorldCapsule> worldCapsules = _desc.enableCollision ? Update_WorldCapsule(model, &ownerWorld) : vector<WorldCapsule>{};

	for (auto &chain : _chains)
	{
		float curDamping = _desc.damping * chain.dampingMul;
		Vec3 curGravity = _desc.gravity * chain.gravityMul;
		const auto isPinnedNode = [this, &chain](const BoneChainNode &node)
			{
				if (node.parentNodeIndex == -1 || node.isVirtualAnchor)
				{
					return true;
				}

				return _desc.pinRoot && chain.nodes[node.parentNodeIndex].isVirtualAnchor;
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

			Vec3 movementOffset = -(node.originalWorld.Translation() - node.prevOriginalPosition) * _desc.movementInertiaScale * chain.movementInertiaMul * movementWeight;
			const float movementOffsetLength = movementOffset.Length();
			const float maxMovementOffset = _desc.maxMovementOffset * movementWeight;
			if (movementOffsetLength > maxMovementOffset && movementOffsetLength > 0.0001f)
			{
				movementOffset /= movementOffsetLength;
				movementOffset *= maxMovementOffset;
			}

			Vec3 nextPosition = Math::SolveVerlet(node.position, node.prevPosition, curGravity, curDamping, timeDelta);
			nextPosition += movementOffset;

			node.prevPosition = node.position;
			node.position = nextPosition;

			if (_desc.poseRestoreStrength > 0.f)
			{
				const float restoreAlpha = std::clamp(_desc.poseRestoreStrength * chain.poseRestoreMul, 0.f, 1.f);
				node.position = Vec3::Lerp(node.position, node.originalWorld.Translation(), restoreAlpha);
			}

			if (_desc.enableCollision)
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

			if (_desc.enableCollision)
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
			Vec3 originalDir = node.originalWorld.Translation() - parentNode.originalWorld.Translation();
			Vec3 simulatedDir = node.position - parentNode.position;

			if (originalDir.LengthSquared() <= 0.0001f || simulatedDir.LengthSquared() <= 0.0001f)
			{
				continue;
			}

			originalDir.Normalize();
			simulatedDir.Normalize();

			const float maxAngle = XMConvertToRadians(_desc.maxBendAngleDegree * chain.maxBendAngleMul);
			const float dot = std::clamp(originalDir.Dot(simulatedDir), -1.f, 1.f);
			const float angle = acosf(dot);
			if (angle > maxAngle && maxAngle > 0.f)
			{
				Vec3 axis = originalDir.Cross(simulatedDir);
				if (axis.LengthSquared() <= 0.0001f)
				{
					continue;
				}

				axis.Normalize();
				const Matrix limitRotation = Matrix::CreateFromAxisAngle(axis, maxAngle);
				const Vec3 limitedDir = Vec3::TransformNormal(originalDir, limitRotation);
				node.position = parentNode.position + limitedDir * node.length;
			}

			if (_desc.enableCollision)
			{
				node.position = Compute_WorldCapsule(node, worldCapsules, node.position);
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
	if (!model)
	{
		return;
	}

	for (auto &chain : _chains)
	{
		const float maxBendAngleDegree = _desc.maxBendAngleDegree * chain.maxBendAngleMul;
		const auto isRootNode = [this, &chain](const BoneChainNode &node)
			{
				return _desc.pinRoot && node.parentNodeIndex >= 0 && chain.nodes[node.parentNodeIndex].isVirtualAnchor;
			};

		for (size_t i = 0; i < chain.nodes.size(); ++i)
		{
			auto &node = chain.nodes[i];

			if (node.isVirtualAnchor || node.parentNodeIndex < 0 || isRootNode(node))
			{
				continue;
			}

			auto &parentNode = chain.nodes[node.parentNodeIndex];
			Vec3 simulatedWorldDir = node.position - parentNode.position;
			if (simulatedWorldDir.LengthSquared() <= 0.0001f)
			{
				continue;
			}

			simulatedWorldDir.Normalize();

			const Vec3 simulatedLocalDir = Get_LocalDirection(parentNode.originalWorld, simulatedWorldDir);

			if (node.bindLocalDir.LengthSquared() <= 0.0001f || simulatedLocalDir.LengthSquared() <= 0.0001f)
			{
				continue;
			}

			const Vec3 limitedLocalDir = Limit_LocalDirectionByAngle(node.bindLocalDir, simulatedLocalDir, maxBendAngleDegree);
			const Quaternion deltaRotation = Build_RotationDelta(node.bindLocalDir, limitedLocalDir);
			const Matrix local = Build_LocalMatrix_RotationOnly(node.bindLocal, deltaRotation);

			model->Set_BoneLocalMatrix(node.boneIndex, local);
		}
	}
}

vector<ChainModifier::WorldCapsule> ChainModifier::Update_WorldCapsule(const ModelCom *model, const Matrix *ownerWorld)
{
	vector<WorldCapsule> worldCapsules;
	worldCapsules.reserve(_colliders.size());

	for (const auto &col : _colliders)
	{
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

		Vec3 localAxis = Vec3::Transform(Vec3(0, 0, 1), col.rotationQuat);
		localAxis.Normalize();

		Vec3 worldCenter = Vec3::Transform(col.center, boneWorld);
		Vec3 worldAxis = Vec3::TransformNormal(localAxis, boneWorld);

		float scale = Vec3(boneWorld.m[0][0], boneWorld.m[0][1], boneWorld.m[0][2]).Length();
		worldAxis.Normalize();

		WorldCapsule capsule{};
		capsule.radius = col.radius * scale;
		float scaledHalfLength = col.halfLength * scale;

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

		const float minCollisionDist = wc.radius + _desc.collisionRadiusPadding;
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
		currentNode.position += offset;
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

	if (!_chainsInitialized)
	{
		Initialize_Chains(model);
	}

	Matrix ownerWorld = _desc.ownerTransform ? _desc.ownerTransform->Get_WorldMatrix() : Matrix::Identity;
	Vec3 currentOwnerPos = ownerWorld.Translation();

	if (_isFirstUpdate)
	{
		_prevOwnerPos = currentOwnerPos;
		ResetPose(model);
		_isFirstUpdate = false;
		return;
	}

	const float ownerDistance = Vec3::Distance(_prevOwnerPos, currentOwnerPos);
	if (ownerDistance > _desc.resetDistance)
	{
		ResetPose(model);
	}

	_prevOwnerPos = currentOwnerPos;

	Capture_CurrentPose(model, ownerWorld);

	const uint32 solverIterations = std::max<uint32>(_desc.solverIterations, 1);
	const float solverTimeDelta = timeDelta / static_cast<float>(solverIterations);
	const float movementWeight = 1.f / static_cast<float>(solverIterations);

	for (uint32 i = 0; i < solverIterations; ++i)
	{
		Solve_Verlet(model, solverTimeDelta, ownerWorld, movementWeight);
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
