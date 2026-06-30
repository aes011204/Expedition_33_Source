#include "PhysicsModifier.h"
#include "ModelCom.h"
#include "ModelPhysicsParser.h"
#include "Reflection_Types.h"
#include "GameInstance.h"
#include "Engine_Macro.h"
#include "Collision_Define.h"
#include <extensions/PxRigidBodyExt.h>
#include <extensions/PxRigidActorExt.h>
using namespace physx;
#include "PhysX_Filter.h"

#include <algorithm>

NS_BEGIN(Engine)

namespace
{
	

	Quat Extract_Rotation(const Matrix& matrix)
	{
		Matrix m = matrix;
		Vec3 scale;
		Quat rotation;
		Vec3 translation;
		m.Decompose(scale, rotation, translation);
		return rotation;
	}

	template<typename Member>
	size_t PhysicsOffset(Member PhysicsModifier::PhysicsModifierDesc::* member)
	{
		PhysicsModifier::PhysicsModifierDesc desc{};
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

	PxVec3 To_PxVec3(const Vec3& value) { return PxVec3(value.x, value.y, value.z); }
	Vec3 From_PxVec3(const PxVec3& value) { return Vec3(value.x, value.y, value.z); }
	PxQuat To_PxQuat(const Quat& value) { return PxQuat(value.x, value.y, value.z, value.w); }
	Quat From_PxQuat(const PxQuat& value) { return Quat(value.x, value.y, value.z, value.w); }

	
	

	Matrix Get_OwnerWorld(const PhysicsModifier::PhysicsModifierDesc& desc)
	{
		if (desc.ownerWorldMatrix) return *desc.ownerWorldMatrix;
		return desc.ownerTransform ? desc.ownerTransform->Get_WorldMatrix() : Matrix::Identity;
	}

	Vec3 Local_Get_Position(const Matrix& matrix) { return Vec3(matrix._41, matrix._42, matrix._43); }

	float SmoothStep(float value)
	{
		value = std::clamp(value, 0.f, 1.f);
		return value * value * (3.f - 2.f * value);
	}

	Vec3 Local_Get_LocalDirection(const Matrix& parentWorld, const Vec3& worldDirection)
	{
		Vec3 localDirection = Vec3::TransformNormal(worldDirection, parentWorld.Invert());
		if (localDirection.LengthSquared() > 0.0001f)
			localDirection.Normalize();
		return localDirection;
	}

	Vec3 Local_Limit_LocalDirectionByAngle(const Vec3& poseLocalDirection, const Vec3& targetLocalDirection, float maxAngleDegree)
	{
		if (maxAngleDegree <= 0.f) return targetLocalDirection;

		Vec3 poseDir = poseLocalDirection;
		Vec3 targetDir = targetLocalDirection;
		if (poseDir.LengthSquared() <= 0.0001f || targetDir.LengthSquared() <= 0.0001f) return poseLocalDirection;

		poseDir.Normalize();
		targetDir.Normalize();
		const float maxAngle = XMConvertToRadians(maxAngleDegree);
		const float dot = std::clamp(poseDir.Dot(targetDir), -1.f, 1.f);
		const float angle = acosf(dot);
		if (angle <= maxAngle) return targetDir;

		Vec3 axis = poseDir.Cross(targetDir);
		if (axis.LengthSquared() <= 0.0001f) return poseDir;
		axis.Normalize();

		Vec3 limitedDir = Vec3::Transform(poseDir, Matrix::CreateFromQuaternion(Quat::CreateFromAxisAngle(axis, maxAngle)));
		if (limitedDir.LengthSquared() > 0.0001f) limitedDir.Normalize();
		return limitedDir;
	}

	Quat Local_Build_RotationDelta(const Vec3& fromDirection, const Vec3& toDirection)
	{
		Vec3 from = fromDirection;
		Vec3 to = toDirection;
		if (from.LengthSquared() <= 0.0001f || to.LengthSquared() <= 0.0001f) return Quat::Identity;

		from.Normalize();
		to.Normalize();
		const float dot = std::clamp(from.Dot(to), -1.f, 1.f);
		if (dot > 0.9995f) return Quat::Identity;

		if (dot < -0.9995f)
		{
			Vec3 axis = Vec3::UnitX.Cross(from);
			if (axis.LengthSquared() <= 0.0001f) axis = Vec3::UnitY.Cross(from);
			if (axis.LengthSquared() <= 0.0001f) return Quat::Identity;
			axis.Normalize();
			return Quat::CreateFromAxisAngle(axis, XM_PI);
		}

		Vec3 axis = from.Cross(to);
		if (axis.LengthSquared() <= 0.0001f) return Quat::Identity;
		axis.Normalize();
		Quat delta = Quat::CreateFromAxisAngle(axis, acosf(dot));
		delta.Normalize();
		return delta;
	}

	bool Build_SegmentWorldFrame(const Vec3& start, const Vec3& end, Matrix& outFrame)
	{
		Vec3 segmentAxis = end - start;
		const float segmentLength = segmentAxis.Length();
		if (segmentLength <= 0.0001f) return false;

		segmentAxis /= segmentLength;
		Quat rotation = Local_Build_RotationDelta(Vec3::UnitX, segmentAxis);
		rotation.Normalize();

		const Vec3 center = (start + end) * 0.5f;
		outFrame = Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(center);
		return true;
	}

	Matrix Local_Build_LocalMatrix_FromSRT(const Vec3& scale, const Quat& rotation, const Vec3& translation)
	{
		Quat normalizedRotation = rotation;
		normalizedRotation.Normalize();

		Matrix local = Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(normalizedRotation);
		local._41 = translation.x;
		local._42 = translation.y;
		local._43 = translation.z;
		local._44 = 1.f;
		return local;
	}

	Vec3 Local_Closest_PointOnSegment(const Vec3& point, const Vec3& start, const Vec3& end)
	{
		Vec3 dir = end - start;
		float lengthSq = dir.LengthSquared();
		if (lengthSq <= 0.0001f) return start;

		float t = std::clamp((point - start).Dot(dir) / lengthSq, 0.f, 1.f);
		return start + dir * t;
	}

	PxQuat Build_AxisAlignmentQuat(const PxVec3& axis)
	{
		PxVec3 from = PxVec3(1.f, 0.f, 0.f);
		float dot = std::clamp(from.dot(axis), -1.f, 1.f);
		if (dot > 0.9999f) return PxQuat(PxIdentity);
		if (dot <= -0.9999f) return PxQuat(PxPi, PxVec3(0.f, 1.f, 0.f));

		PxVec3 rotAxis = from.cross(axis);
		if (rotAxis.magnitudeSquared() <= 0.000001f) rotAxis = PxVec3(0.f, 1.f, 0.f);
		rotAxis.normalize();
		return PxQuat(acosf(dot), rotAxis);
	}

	PxQuat Build_JointFrameQuat(const Vec3& tangentWorld, const Vec3& referenceUpWorld)
	{
		Vec3 tangent = tangentWorld;
		if (tangent.LengthSquared() <= 0.000001f) tangent = Vec3::UnitX;
		else tangent.Normalize();

		const Quat baseRotation = From_PxQuat(Build_AxisAlignmentQuat(To_PxVec3(tangent)));
		Vec3 currentUp = Vec3::TransformNormal(Vec3::UnitY, Matrix::CreateFromQuaternion(baseRotation));
		if (currentUp.LengthSquared() <= 0.000001f) currentUp = Vec3::UnitY;
		currentUp.Normalize();

		Vec3 desiredUp = referenceUpWorld - tangent * referenceUpWorld.Dot(tangent);
		if (desiredUp.LengthSquared() <= 0.000001f)
		{
			desiredUp = Vec3::UnitZ - tangent * Vec3::UnitZ.Dot(tangent);
			if (desiredUp.LengthSquared() <= 0.000001f)
				desiredUp = Vec3::UnitY - tangent * Vec3::UnitY.Dot(tangent);
		}
		if (desiredUp.LengthSquared() <= 0.000001f) return To_PxQuat(baseRotation);
		desiredUp.Normalize();

		const float dot = std::clamp(currentUp.Dot(desiredUp), -1.f, 1.f);
		float angle = acosf(dot);
		if (angle <= 0.0001f) return To_PxQuat(baseRotation);

		const float sign = (currentUp.Cross(desiredUp)).Dot(tangent) < 0.f ? -1.f : 1.f;
		const Quat twist = Quat::CreateFromAxisAngle(tangent, angle * sign);
		Quat finalRotation = twist * baseRotation;
		finalRotation.Normalize();
		return To_PxQuat(finalRotation);
	}

	void Configure_ShapeFilter(PxShape* shape, Collision_Channel channel, uint32 overlapMask, uint32 blockMask)
	{
		if (shape == nullptr) return;
		PxFilterData filterData{};
		filterData.word0 = ETOI(channel);
		filterData.word1 = overlapMask;
		filterData.word2 = blockMask;
		filterData.word3 = 0;
		shape->setSimulationFilterData(filterData);
		shape->setQueryFilterData(filterData);
	}
}

Vec3 PhysicsModifier::Get_Position(const Matrix& matrix)
{
	return Local_Get_Position(matrix);
}

Vec3 PhysicsModifier::Get_LocalDirection(const Matrix& parentWorld, const Vec3& worldDirection)
{
	return Local_Get_LocalDirection(parentWorld, worldDirection);
}

Vec3 PhysicsModifier::Limit_LocalDirectionByAngle(const Vec3& baseLocalDirection, const Vec3& targetLocalDirection, float maxAngleDegree)
{
	return Local_Limit_LocalDirectionByAngle(baseLocalDirection, targetLocalDirection, maxAngleDegree);
}

Quat PhysicsModifier::Build_RotationDelta(const Vec3& fromDirection, const Vec3& toDirection)
{
	return Local_Build_RotationDelta(fromDirection, toDirection);
}

Matrix PhysicsModifier::Build_LocalMatrix_FromSRT(const Vec3& scale, const Quat& rotation, const Vec3& translation)
{
	return Local_Build_LocalMatrix_FromSRT(scale, rotation, translation);
}

Vec3 PhysicsModifier::Closest_PointOnSegment(const Vec3& point, const Vec3& start, const Vec3& end)
{
	return Local_Closest_PointOnSegment(point, start, end);
}

Shared<PhysicsModifier> PhysicsModifier::Create(const PhysicsModifierDesc* desc)
{
	auto modifier = make_shared<PhysicsModifier>();
	if (FAILED(modifier->Initialize(const_cast<PhysicsModifierDesc*>(desc))))
		return nullptr;
	return modifier;
}

HRESULT PhysicsModifier::Initialize(AnimationModifierDesc* baseDesc)
{
	if (baseDesc == nullptr) return E_FAIL;
	auto* desc = static_cast<PhysicsModifierDesc*>(baseDesc);

	_desc = *desc;
	_desc.chains = desc->chains;
	_desc.includedCollisionBones = desc->includedCollisionBones;
	_desc.overrideCapsules = desc->overrideCapsules;
	_desc.windDirection = desc->windDirection;
	_desc.windScale = desc->windScale;
	
	if (!_desc.physicsAssetPath.empty())
	{
		_colliders = ModelPhysicsParser::LoadCapsulesFromJSON(_desc.physicsAssetPath);
	}
	for (const auto& cap : _desc.overrideCapsules)
	{
		_colliders.push_back(cap);
	}

	for (const auto& chainDesc : _desc.chains)
	{
		_configuredRoots.insert(chainDesc.rootBoneName);
	}

	return S_OK;
}

void PhysicsModifier::On_ActiveChanged(ModelCom* model, bool isActive)
{
	if (isActive)
	{
		ResetPose(model);
	}
	else
	{
		Restore_PoseLocals(model);
	}
}

void PhysicsModifier::Evaluate_Modifier(ModelCom* model, float timeDelta)
{
	if (!_isActive || _strengthAlpha <= 0.001f || model == nullptr)
		return;

	model->Rebuild_BoneCombinedMatrices();

	if (!_chainsInitialized)
	{
		Initialize_Chains(model);
		_chainsInitialized = true;
	}

	if (_chains.empty()) return;

	Matrix ownerWorld = Get_OwnerWorld(_desc);
	if (_runtimeTestOwnerMotionEnabled)
	{
		ownerWorld._41 += _runtimeTestOwnerMotionDelta.x;
		ownerWorld._42 += _runtimeTestOwnerMotionDelta.y;
		ownerWorld._43 += _runtimeTestOwnerMotionDelta.z;
	}
	Vec3 ownerPos = Get_Position(ownerWorld);

	if (_hasPrevOwnerWorld)
	{
		if (Vec3::Distance(ownerPos, _prevOwnerPos) >= _desc.teleportDistance)
		{
			ResetPose(model);
		}
	}

	Capture_CurrentPose(model, ownerWorld);

	if (Ensure_PhysXResources(model, _desc.collisionModel, ownerWorld, _desc.collisionOwnerWorldMatrix ? *_desc.collisionOwnerWorldMatrix : ownerWorld))
	{
		Sync_KinematicActors(model, _desc.collisionModel, ownerWorld, _desc.collisionOwnerWorldMatrix ? *_desc.collisionOwnerWorldMatrix : ownerWorld);
		
		if (_desc.windScale > 0.001f && _desc.windDirection.LengthSquared() > 0.001f)
		{
			Apply_WindForce(_desc.windDirection, _desc.windScale);
		}

		if (_scene)
		{
			_scene->simulate(timeDelta);
			_scene->fetchResults(true);
		}

		Update_SolvedPositionsFromActors();

		vector<WorldCapsule> worldCapsules = Build_WorldCapsules(_desc.collisionModel, _desc.collisionOwnerWorldMatrix ? *_desc.collisionOwnerWorldMatrix : ownerWorld);
		
		Apply_PostSimulationConstraints(model, ownerWorld, worldCapsules, timeDelta);

		bool isOwnerIdle = false;
		if (_hasPrevOwnerWorld)
			isOwnerIdle = Vec3::Distance(ownerPos, _prevOwnerPos) <= _desc.idleMotionDeadzone;

		if (_desc.writeBackEnabled)
			Write_BackToModel(model, ownerWorld, isOwnerIdle);

		Build_DebugOutput(model, ownerWorld);
	}

	_prevOwnerWorld = ownerWorld;
	_prevOwnerPos = ownerPos;
	_hasPrevOwnerWorld = true;
}

void PhysicsModifier::Free()
{
	Release_PhysXResources();
	AnimationModifier::Free();
}

void PhysicsModifier::RuntimeRebuild(ModelCom* model)
{
	Release_PhysXResources();
	_chains.clear();
	_chainsInitialized = false;
	Initialize_Chains(model);
}

void PhysicsModifier::ResetPose(ModelCom* model)
{
	if (!_chainsInitialized || _chains.empty() || !model) return;
	
	model->Rebuild_BoneCombinedMatrices();
	Matrix ownerWorld = Get_OwnerWorld(_desc);
	Reset_PhysXStateToPose(model, _desc.collisionModel, ownerWorld, _desc.collisionOwnerWorldMatrix ? *_desc.collisionOwnerWorldMatrix : ownerWorld);
	_prevOwnerPos = Get_Position(ownerWorld);
	_prevOwnerWorld = ownerWorld;
	_hasPrevOwnerWorld = true;
}

void PhysicsModifier::Restore_PoseLocals(ModelCom* model)
{
	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			model->Set_BoneLocalMatrix(node.boneIndex, node.poseLocal);
		}
	}
	model->Rebuild_BoneCombinedMatrices();
}


void PhysicsModifier::Apply_WindForce(const Vec3& direction, float scale)
{
	if (!_scene || direction.LengthSquared() <= 0.0001f || scale <= 0.0001f)
		return;

	PxVec3 force = To_PxVec3(direction) * scale;
	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			if (node.isKinematic || !node.actor)
				continue;
			node.actor->addForce(force, PxForceMode::eFORCE);
		}
	}
}

void PhysicsModifier::Set_RuntimeTestOwnerMotion(bool enabled, const Vec3& delta)
{
	_runtimeTestOwnerMotionEnabled = enabled;
	_runtimeTestOwnerMotionDelta = enabled ? delta : Vec3::Zero;
}

void PhysicsModifier::Release_PhysXResources()
{
	if (_scene)
	{
		for (auto& blocker : _blockers)
		{
			if (blocker.actor) _scene->removeActor(*blocker.actor);
		}
		for (auto& chain : _chains)
		{
			for (auto& node : chain.nodes)
			{
				if (node.actor) _scene->removeActor(*node.actor);
			}
			if (chain.anchorActor) _scene->removeActor(*chain.anchorActor);
		}
	}

	for (auto& blocker : _blockers)
	{
		Safe_Release(blocker.actor);
	}
	_blockers.clear();

	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			Safe_Release(node.joint);
			Safe_Release(node.actor);
		}
		Safe_Release(chain.anchorActor);
	}

	Safe_Release(_material);
	
	if (_scene)
	{
		if (auto dispatcher = static_cast<PxDefaultCpuDispatcher*>(_scene->getCpuDispatcher())) dispatcher->release();
		_scene->release();
		_scene = nullptr;
	}

	_physics = nullptr;
	_physxResourcesReady = false;
}

void PhysicsModifier::Initialize_Chains(ModelCom* model)
{
	if (model == nullptr) return;

	_chainsInitialized = true;
	for (auto& desc : _desc.chains)
	{
		uint32 rootIndex = model->Get_BoneIndex(desc.rootBoneName);
		if (rootIndex == -1) continue;

		Chain chain{};
		chain.desc = desc;
		chain.anchorBoneIndex = model->Get_BoneParentIndex(rootIndex);
		
		BuildChainRecursive(model, rootIndex, -1, chain);

		if (chain.nodes.size() > 1)
		{
			for (size_t i = 0; i < chain.nodes.size(); ++i)
			{
				chain.nodes[i].depthRatio = static_cast<float>(i) / static_cast<float>(chain.nodes.size() - 1);
			}
			_chains.push_back(std::move(chain));
		}
	}
}

void PhysicsModifier::BuildChainRecursive(ModelCom* model, int32 boneIndex, int32 parentNodeIndex, Chain& outChain)
{
	if (boneIndex == -1) return;

	Node node{};
	node.boneIndex = boneIndex;
	node.parentNodeIndex = parentNodeIndex;
	node.bindLocal = model->Get_BoneLocalMatrix(boneIndex);
	node.poseLocal = model->Get_BoneLocalMatrix(boneIndex);
	node.bindLocalDir = Get_LocalDirection(Matrix::Identity, Get_Position(node.bindLocal));

	int32 nodeIndex = static_cast<int32>(outChain.nodes.size());
	outChain.nodes.push_back(node);

	if (parentNodeIndex >= 0)
	{
		outChain.nodes[parentNodeIndex].childNodeIndices.push_back(nodeIndex);
	}

	for (uint32 childBoneIndex = 0; childBoneIndex < model->Get_NumBones(); ++childBoneIndex)
	{
		if (model->Get_BoneParentIndex(childBoneIndex) == boneIndex)
		{
			BuildChainRecursive(model, childBoneIndex, nodeIndex, outChain);
		}
	}
}

void PhysicsModifier::Capture_CurrentPose(ModelCom* model, const Matrix& ownerWorld)
{
	for (auto& chain : _chains)
	{
		chain.anchorWorld = (*model->Get_BoneMatrixPtr(chain.anchorBoneIndex)) * ownerWorld;
		for (auto& node : chain.nodes)
		{
			node.poseLocal = model->Get_BoneLocalMatrix(node.boneIndex);
			node.originalWorld = (*model->Get_BoneMatrixPtr(node.boneIndex)) * ownerWorld;
			
			if (node.parentNodeIndex >= 0)
			{
				Matrix parentWorld = chain.nodes[node.parentNodeIndex].originalWorld;
				node.length = Vec3::Distance(Get_Position(node.originalWorld), Get_Position(parentWorld));
			}
		}
	}
}

bool PhysicsModifier::Ensure_PhysXResources(ModelCom* model, const ModelCom* collisionModel, const Matrix& ownerWorld, const Matrix& collisionOwnerWorld)
{
	if (_physxResourcesReady) return true;

	_physics = GAME->Get_PhysXPhysics();
	if (!_physics) return false;

	PxSceneDesc sceneDesc(_physics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
	sceneDesc.cpuDispatcher = PxDefaultCpuDispatcherCreate(1);
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;
	_scene = _physics->createScene(sceneDesc);

	_material = _physics->createMaterial(0.2f, 0.2f, 0.0f);

	for (auto& chain : _chains)
	{
		PxTransform anchorPose(To_PxVec3(Get_Position(chain.anchorWorld)), To_PxQuat(Extract_Rotation(chain.anchorWorld)));
		chain.anchorActor = _physics->createRigidDynamic(anchorPose);
		chain.anchorActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
		chain.anchorActor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);
		_scene->addActor(*chain.anchorActor);

		for (size_t i = 0; i < chain.nodes.size(); ++i)
		{
			auto& node = chain.nodes[i];
			node.isKinematic = (node.parentNodeIndex < 0);
			
			PxTransform nodePose(To_PxVec3(Get_Position(node.originalWorld)), To_PxQuat(Extract_Rotation(node.originalWorld)));
			node.actor = _physics->createRigidDynamic(nodePose);
			
			if (node.isKinematic)
			{
				node.actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
				node.actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);
			}
			else
			{
				float chainRadiusMul = chain.desc.radiusMul;
				float baseNodeRadius = max(0.05f, _desc.collisionSphereRadius * 0.65f);
				node.debugSphereRadius = max(0.035f, min(baseNodeRadius * chainRadiusMul, max(0.045f, node.length * 0.34f * chainRadiusMul)));
				
				PxShape* shape = _physics->createShape(PxSphereGeometry(node.debugSphereRadius), *_material, true);
				node.actor->attachShape(*shape);
				shape->release();
				Configure_ShapeFilter(shape, Collision_Channel::Monster_Attack, 0, ETOI(Collision_Channel::Monster_Body));

				node.actor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
				node.actor->setLinearDamping((8.50f - node.depthRatio * 3.20f) * chain.desc.dampingMul);
				node.actor->setAngularDamping((9.50f - node.depthRatio * 3.60f) * chain.desc.dampingMul);
				node.actor->setSolverIterationCounts(max(12u, _desc.solverIterations * 3), 4);
				node.actor->setMaxLinearVelocity(7.5f);
				node.actor->setMaxAngularVelocity(5.5f);
				PxRigidBodyExt::updateMassAndInertia(*node.actor, 0.08f + node.depthRatio * 0.03f);
			}
			_scene->addActor(*node.actor);

			if (!node.isKinematic)
			{
				PxRigidDynamic* parentActor = (node.parentNodeIndex >= 0) ? chain.nodes[node.parentNodeIndex].actor : chain.anchorActor;
				
				Vec3 tangent = Get_Position(node.originalWorld) - From_PxVec3(parentActor->getGlobalPose().p);
				PxQuat jointFrame = Build_JointFrameQuat(tangent, Vec3::UnitY);
				PxTransform parentLocalFrame(To_PxVec3(Vec3::Zero), jointFrame);
				PxTransform childLocalFrame(To_PxVec3(-tangent), jointFrame);

				node.joint = PxSphericalJointCreate(*_physics, parentActor, parentLocalFrame, node.actor, childLocalFrame);
				
				float maxAngle = _desc.maxOutputAngleDegree * chain.desc.maxAngleMul;
				float swingLimitDegree = std::clamp(min(maxAngle, 90.f) * (0.32f + node.depthRatio * 0.48f), 3.5f, 75.f);
				float swingLimit = XMConvertToRadians(swingLimitDegree);
				
				PxJointLimitCone limitCone(swingLimit * 0.5f, swingLimit);
				limitCone.restitution = 0.f;
				limitCone.stiffness = 0.f;
				limitCone.damping = 0.f;
				
				static_cast<PxSphericalJoint*>(node.joint)->setLimitCone(limitCone);
				static_cast<PxSphericalJoint*>(node.joint)->setSphericalJointFlag(PxSphericalJointFlag::eLIMIT_ENABLED, true);
				node.joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, false);
				node.joint->setInvMassScale0(0.0f);
				node.joint->setInvInertiaScale0(0.0f);
				node.joint->setInvMassScale1(1.0f);
				node.joint->setInvInertiaScale1(1.0f);
			}
		}
	}

	vector<WorldCapsule> caps = Build_WorldCapsules(collisionModel, collisionOwnerWorld);
	for (size_t i = 0; i < caps.size(); ++i)
	{
		Blocker blocker{};
		blocker.radius = caps[i].radius;
		float halfHeight = Vec3::Distance(caps[i].startPoint, caps[i].endPoint) * 0.5f;
		blocker.halfHeight = halfHeight;
		blocker.isSphere = (halfHeight <= 0.0001f);
		
		Vec3 center = (caps[i].startPoint + caps[i].endPoint) * 0.5f;
		Matrix frame;
		if (!Build_SegmentWorldFrame(caps[i].startPoint, caps[i].endPoint, frame))
			frame = Matrix::CreateTranslation(center);

		PxTransform blockerPose(To_PxVec3(center), To_PxQuat(Extract_Rotation(frame)));
		blocker.actor = _physics->createRigidDynamic(blockerPose);
		blocker.actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
		blocker.actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);

		PxGeometry* geom = nullptr;
		PxSphereGeometry sphereGeom(blocker.radius);
		PxCapsuleGeometry capGeom(blocker.radius, blocker.halfHeight);
		
		if (blocker.isSphere) geom = &sphereGeom;
		else geom = &capGeom;

		PxShape* shape = PxRigidActorExt::createExclusiveShape(*blocker.actor, *geom, *_material);
		Configure_ShapeFilter(shape, Collision_Channel::Monster_Body, 0, ETOI(Collision_Channel::Monster_Attack));
		
		_scene->addActor(*blocker.actor);
		_blockers.push_back(blocker);
	}

	_physxResourcesReady = true;
	return true;
}

void PhysicsModifier::Reset_PhysXStateToPose(ModelCom* model, const ModelCom* collisionModel, const Matrix& ownerWorld, const Matrix& collisionOwnerWorld)
{
	Capture_CurrentPose(model, ownerWorld);
	Sync_KinematicActors(model, collisionModel, ownerWorld, collisionOwnerWorld);
	
	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			if (!node.isKinematic && node.actor)
			{
				PxTransform pose(To_PxVec3(Get_Position(node.originalWorld)), To_PxQuat(Extract_Rotation(node.originalWorld)));
				node.actor->setGlobalPose(pose);
				node.actor->setLinearVelocity(PxVec3(0.f));
				node.actor->setAngularVelocity(PxVec3(0.f));
			}
			node.prevSolvedPosition = Get_Position(node.originalWorld);
			node.smoothedLocalRotation = Extract_Rotation(node.poseLocal);
			node.smoothedCollisionOffset = Vec3::Zero;
			node.smoothedContactNormal = Vec3::Zero;
		}
	}
}

void PhysicsModifier::Sync_KinematicActors(ModelCom* model, const ModelCom* collisionModel, const Matrix& ownerWorld, const Matrix& collisionOwnerWorld)
{
	for (auto& chain : _chains)
	{
		PxTransform anchorPose(To_PxVec3(Get_Position(chain.anchorWorld)), To_PxQuat(Extract_Rotation(chain.anchorWorld)));
		if (chain.anchorActor)
		{
			chain.anchorActor->setKinematicTarget(anchorPose);
			chain.anchorActor->wakeUp();
		}

		for (auto& node : chain.nodes)
		{
			node.hadContactThisFrame = false;
			node.hadPushThisFrame = false;
			if (node.isKinematic && node.actor)
			{
				PxTransform nodePose(To_PxVec3(Get_Position(node.originalWorld)), To_PxQuat(Extract_Rotation(node.originalWorld)));
				node.actor->setKinematicTarget(nodePose);
			}
			if (node.actor) node.actor->wakeUp();
		}
	}

	vector<WorldCapsule> caps = Build_WorldCapsules(collisionModel, collisionOwnerWorld);
	for (size_t i = 0; i < min(caps.size(), _blockers.size()); ++i)
	{
		Vec3 center = (caps[i].startPoint + caps[i].endPoint) * 0.5f;
		Matrix frame;
		if (!Build_SegmentWorldFrame(caps[i].startPoint, caps[i].endPoint, frame))
			frame = Matrix::CreateTranslation(center);

		PxTransform blockerPose(To_PxVec3(center), To_PxQuat(Extract_Rotation(frame)));
		if (_blockers[i].actor)
		{
			_blockers[i].actor->setKinematicTarget(blockerPose);
			_blockers[i].actor->wakeUp();
		}
	}
}

void PhysicsModifier::Update_SolvedPositionsFromActors()
{
	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			node.prevSolvedPosition = node.solvedPosition;
			if (node.actor)
			{
				node.solvedPosition = From_PxVec3(node.actor->getGlobalPose().p);
			}
			else
			{
				node.solvedPosition = Get_Position(node.originalWorld);
			}
		}
	}
}

void PhysicsModifier::Apply_PostSimulationConstraints(ModelCom* model, const Matrix& ownerWorld, const vector<WorldCapsule>& worldCapsules, float timeDelta)
{
	if (!_desc.enableCollision || worldCapsules.empty()) return;

	float skinWidth = _desc.collisionContactSkinWidth;
	float pushOutScale = _desc.collisionPushOutScale;
	uint32 iterations = _desc.collisionProjectionIterations;

	for (auto& chain : _chains)
	{
		for (uint32 iter = 0; iter < iterations; ++iter)
		{
			for (size_t i = 0; i < chain.nodes.size(); ++i)
			{
				auto& node = chain.nodes[i];
				if (node.isKinematic) continue;

				float combinedRadius = node.debugSphereRadius + skinWidth;
				Vec3 position = node.solvedPosition;
				bool resolved = false;

				for (const auto& capsule : worldCapsules)
				{
					Vec3 closest = Closest_PointOnSegment(position, capsule.startPoint, capsule.endPoint);
					Vec3 delta = position - closest;
					float dist = delta.Length();
					float targetDist = combinedRadius + capsule.radius;

					if (dist < targetDist)
					{
						if (dist > 0.0001f)
						{
							Vec3 normal = delta / dist;
							float penetration = targetDist - dist;
							float depenetration = penetration * (0.82f + pushOutScale * 0.12f) + 0.0005f;
							position += normal * depenetration;
						}
						else
						{
							position += Vec3::UnitY * (targetDist + 0.0005f);
						}
						node.hadContactThisFrame = true;
						node.hadPushThisFrame = true;
						resolved = true;
					}
				}
				if (resolved)
				{
					node.solvedPosition = position;
					if (node.actor)
					{
						PxTransform pose = node.actor->getGlobalPose();
						pose.p = To_PxVec3(position);
						node.actor->setGlobalPose(pose);
						node.actor->setLinearVelocity(node.actor->getLinearVelocity() * (1.f - _desc.contactInertiaDamping));
					}
				}
			}
		}
	}
}

void PhysicsModifier::Write_BackToModel(ModelCom* model, const Matrix& ownerWorld, bool isOwnerIdle)
{
	float smoothAlpha = isOwnerIdle ? _desc.idleOutputSmoothAlpha : _desc.outputSmoothAlpha;

	for (auto& chain : _chains)
	{
		for (size_t i = 0; i < chain.nodes.size(); ++i)
		{
			auto& node = chain.nodes[i];
			if (node.isKinematic) continue;

			Matrix parentWorld;
			if (node.parentNodeIndex >= 0)
			{
				parentWorld = chain.nodes[node.parentNodeIndex].originalWorld;
			}
			else
			{
				parentWorld = chain.anchorWorld;
			}

			Vec3 targetLocalDir;
			if (node.childNodeIndices.empty())
			{
				targetLocalDir = Get_LocalDirection(parentWorld, node.solvedPosition - Get_Position(parentWorld));
			}
			else
			{
				targetLocalDir = Get_LocalDirection(parentWorld, node.solvedPosition - Get_Position(parentWorld));
			}

			float nodeAngleLimit = _desc.maxOutputAngleDegree * chain.desc.maxAngleMul * (0.45f + node.depthRatio * 0.55f);
			targetLocalDir = Limit_LocalDirectionByAngle(node.bindLocalDir, targetLocalDir, nodeAngleLimit);

			Quat poseRotation = Extract_Rotation(node.poseLocal);
			Quat solvedRotation = Build_RotationDelta(node.bindLocalDir, targetLocalDir) * poseRotation;

			float nodeRotationOutputAlpha = _desc.outputAlpha * chain.desc.outputMultiplier * _desc.rotationOutputAlpha;
			Quat blendedRotation = Quat::Slerp(poseRotation, solvedRotation, nodeRotationOutputAlpha);

			float rotationFollowAlpha = node.hadContactThisFrame ? 
				_desc.contactRotationFollowAlpha - node.depthRatio * _desc.rotationFollowDepthScale :
				_desc.freeRotationFollowAlpha - node.depthRatio * (_desc.rotationFollowDepthScale + 0.02f);
			
			rotationFollowAlpha = max(0.02f, rotationFollowAlpha * chain.desc.smoothMultiplier);
			
			if (node.hasSmoothedLocalTransform)
			{
				node.smoothedLocalRotation = Quat::Slerp(node.smoothedLocalRotation, blendedRotation, rotationFollowAlpha);
			}
			else
			{
				node.smoothedLocalRotation = blendedRotation;
				node.hasSmoothedLocalTransform = true;
			}

			Vec3 translation = Get_Position(node.poseLocal);
			Matrix finalLocal = Build_LocalMatrix_FromSRT(node.bindScale, node.smoothedLocalRotation, translation);
			
			model->Set_BoneLocalMatrix(node.boneIndex, finalLocal);
			model->Rebuild_BoneCombinedMatrices();
			node.originalWorld = (*model->Get_BoneMatrixPtr(node.boneIndex)) * ownerWorld;
		}
	}
}

vector<PhysicsModifier::WorldCapsule> PhysicsModifier::Build_WorldCapsules(const ModelCom* collisionModel, const Matrix& collisionOwnerWorld) const
{
	vector<WorldCapsule> capsules;
	if (collisionModel == nullptr || _colliders.empty()) return capsules;

	for (const auto& collider : _colliders)
	{
		if (!_desc.includedCollisionBones.empty())
		{
			if (std::find(_desc.includedCollisionBones.begin(), _desc.includedCollisionBones.end(), collider.boneName) == _desc.includedCollisionBones.end())
			{
				continue;
			}
		}

		uint32 boneIndex = collisionModel->Get_BoneIndex(collider.boneName);
		if (boneIndex == -1) continue;

		Matrix boneWorld = (*collisionModel->Get_BoneMatrixPtr(boneIndex)) * collisionOwnerWorld;
		Vec3 center = Vec3::Transform(collider.center, boneWorld);
		Vec3 axis = Vec3::TransformNormal(Vec3::TransformNormal(Vec3::UnitY, Matrix::CreateFromQuaternion(collider.rotationQuat)), boneWorld);
		if (axis.LengthSquared() > 0.0001f) axis.Normalize();

		float radiusScale = max({ 1.0f, 1.0f, 1.0f }); // Assuming uniform scale for now
		float scaledHalfHeight = collider.halfLength * radiusScale;
		
		WorldCapsule cap;
		cap.radius = collider.radius * _desc.collisionPrimitiveScale * radiusScale + _desc.collisionRadiusPadding;
		cap.startPoint = center - axis * scaledHalfHeight;
		cap.endPoint = center + axis * scaledHalfHeight;
		capsules.push_back(cap);
	}
	return capsules;
}

void PhysicsModifier::Build_DebugOutput(ModelCom* model, const Matrix& ownerWorld)
{
	_runtimeDebugCapsules.clear();
	_runtimeDebugSpheres.clear();
	_runtimeDebugSegments.clear();
	// Logic to fill debug visualization vectors can be implemented here if needed.
}

bool PhysicsModifier::Validate_RootAlignment(ModelCom* model, const Matrix& ownerWorld)
{
	return true; // Simplified for this rewrite. Add rigorous check if necessary.
}

const ClassReflectionInfo* PhysicsModifier::Get_RuntimeReflectionInfo() const
{
	static ClassReflectionInfo info{};
	static bool initialized = false;
	if (!initialized)
	{
		info.className = "PhysicsModifierDesc";
		AddRuntimeProperty(info.properties, PropertyType::Bool, "writeBackEnabled", "Write Back Enabled", PhysicsOffset(&PhysicsModifierDesc::writeBackEnabled));
		AddRuntimeProperty(info.properties, PropertyType::Bool, "enableCollision", "Enable Collision", PhysicsOffset(&PhysicsModifierDesc::enableCollision));
		AddRuntimeProperty(info.properties, PropertyType::Float, "outputAlpha", "Output Alpha", PhysicsOffset(&PhysicsModifierDesc::outputAlpha), 0.01f, 0.f, 1.f);
		AddRuntimeProperty(info.properties, PropertyType::Float, "rotationOutputAlpha", "Rotation Output Alpha", PhysicsOffset(&PhysicsModifierDesc::rotationOutputAlpha), 0.01f, 0.f, 1.f);
		AddRuntimeProperty(info.properties, PropertyType::Float, "contactInertiaDamping", "Contact Inertia Damping", PhysicsOffset(&PhysicsModifierDesc::contactInertiaDamping), 0.01f, 0.f, 1.f);
		AddRuntimeProperty(info.properties, PropertyType::Float, "releaseInertiaDamping", "Release Inertia Damping", PhysicsOffset(&PhysicsModifierDesc::releaseInertiaDamping), 0.01f, 0.f, 1.f);
		initialized = true;
	}
	return &info;
}

void PhysicsModifier::On_RuntimeReflectionChanged(ModelCom* model, const PropertyInfo& property)
{
	if (_isActive)
	{
		Restore_PoseLocals(model);
		ResetPose(model);
	}
}

NS_END


