#include "PhysicsModifier.h"
#include "ModelCom.h"
#include "ModelPhysicsParser.h"
#include "Reflection_Types.h"
#include "GameInstance.h"
#include "Engine_Macro.h"
#include "Collision_Define.h"
#include "PhysX_Filter.h"
#include <extensions/PxRigidBodyExt.h>
#include <extensions/PxRigidActorExt.h>
using namespace physx;


#include <algorithm>
#include <cmath>
#include <fstream>

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

	string EscapeCsv(const string& value)
	{
		string escaped;
		escaped.reserve(value.size() + 2);
		escaped.push_back('"');
		for (char ch : value)
		{
			if (ch == '"')
				escaped.push_back('"');
			escaped.push_back(ch);
		}
		escaped.push_back('"');
		return escaped;
	}

	Matrix Local_With_Position(const Matrix& matrix, const Vec3& position)
	{
		Matrix result = matrix;
		result._41 = position.x;
		result._42 = position.y;
		result._43 = position.z;
		return result;
	}

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

	template<typename T>
	void Release_PhysX(T*& object)
	{
		if (object == nullptr) return;
		object->release();
		object = nullptr;
	}

	bool Has_LocalOffsetLimit(const Vec3& limit)
	{
		return limit.x > 0.0001f || limit.y > 0.0001f || limit.z > 0.0001f;
	}

	Vec3 Clamp_LocalOffset(const Vec3& offset, const Vec3& limit, bool& outClamped)
	{
		Vec3 clamped = offset;
		outClamped = false;

		if (limit.x > 0.0001f)
		{
			const float next = std::clamp(offset.x, -limit.x, limit.x);
			outClamped = outClamped || fabsf(next - offset.x) > 0.000001f;
			clamped.x = next;
		}
		if (limit.y > 0.0001f)
		{
			const float next = std::clamp(offset.y, -limit.y, limit.y);
			outClamped = outClamped || fabsf(next - offset.y) > 0.000001f;
			clamped.y = next;
		}
		if (limit.z > 0.0001f)
		{
			const float next = std::clamp(offset.z, -limit.z, limit.z);
			outClamped = outClamped || fabsf(next - offset.z) > 0.000001f;
			clamped.z = next;
		}

		return clamped;
	}

	Vec3 Clamp_VectorLength(const Vec3& value, float maxLength)
	{
		if (maxLength <= 0.0001f)
			return value;

		const float length = value.Length();
		if (length <= maxLength || length <= 0.0001f)
			return value;

		return value * (maxLength / length);
	}

	float SegmentSegmentClosestPoints(
		const Vec3& p1,
		const Vec3& q1,
		const Vec3& p2,
		const Vec3& q2,
		Vec3& outC1,
		Vec3& outC2,
		float& outT1,
		float& outT2)
	{
		const Vec3 d1 = q1 - p1;
		const Vec3 d2 = q2 - p2;
		const Vec3 r = p1 - p2;
		const float a = d1.Dot(d1);
		const float e = d2.Dot(d2);
		const float f = d2.Dot(r);
		float s = 0.f;
		float t = 0.f;

		if (a <= 0.000001f && e <= 0.000001f)
		{
			outC1 = p1;
			outC2 = p2;
			outT1 = 0.f;
			outT2 = 0.f;
			return (outC1 - outC2).LengthSquared();
		}

		if (a <= 0.000001f)
		{
			t = std::clamp(f / e, 0.f, 1.f);
		}
		else
		{
			const float c = d1.Dot(r);
			if (e <= 0.000001f)
			{
				s = std::clamp(-c / a, 0.f, 1.f);
			}
			else
			{
				const float b = d1.Dot(d2);
				const float denom = a * e - b * b;
				if (fabsf(denom) > 0.000001f)
					s = std::clamp((b * f - c * e) / denom, 0.f, 1.f);

				t = (b * s + f) / e;
				if (t < 0.f)
				{
					t = 0.f;
					s = std::clamp(-c / a, 0.f, 1.f);
				}
				else if (t > 1.f)
				{
					t = 1.f;
					s = std::clamp((b - c) / a, 0.f, 1.f);
				}
			}
		}

		outC1 = p1 + d1 * s;
		outC2 = p2 + d2 * t;
		outT1 = s;
		outT2 = t;
		return (outC1 - outC2).LengthSquared();
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

float PhysicsModifier::Compute_NodeAngleLimitDegree(const Chain& chain, float depthRatio) const
{
	const float baseAngle = std::max(0.f, _desc.maxOutputAngleDegree);
	const float tipMultiplier = std::max(0.f, chain.desc.maxAngleMul);
	const float depth = SmoothStep(std::clamp(depthRatio, 0.f, 1.f));
	const float depthMultiplier = std::max(0.f, 1.f + depth * (tipMultiplier - 1.f));
	return std::clamp(baseAngle * depthMultiplier, 0.f, 175.f);
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
	CHECK_FAILED(__super::Initialize(baseDesc), E_FAIL);
	auto* desc = static_cast<PhysicsModifierDesc*>(baseDesc);

	_desc = *desc;
	
	if (!_desc.physicsAssetPath.empty())
	{
		_colliders = ModelPhysicsParser::LoadCapsulesFromJSON(_desc.physicsAssetPath);
	}
	for (const auto& cap : _desc.overrideCapsules)
	{
		_colliders.push_back(cap);
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

	Restore_PoseLocals(model);
	model->Rebuild_BoneCombinedMatrices();

	_desc.debugCollisionHitCount = 0;
	_desc.debugConstraintProjectionCount = 0;
	_desc.debugLocalClampCount = 0;
	_desc.debugLocalClampXCount = 0;
	_desc.debugLocalClampYCount = 0;
	_desc.debugLocalClampZCount = 0;
	_desc.debugInertiaMagnitude = 0.f;
	_desc.debugTraceMaxNodeMotion = 0.f;
	_desc.debugTraceMaxOutputAngle = 0.f;

	Matrix ownerWorld = Get_OwnerWorld(_desc);
	Vec3 ownerPos = Get_Position(ownerWorld);
	Vec3 ownerMotionDelta = Vec3::Zero;
	if (_hasPrevOwnerWorld)
	{
		ownerMotionDelta = ownerPos - _prevOwnerPos;
	}
	bool isOwnerIdle =
		_hasPrevOwnerWorld &&
		ownerMotionDelta.Length() <= _desc.idleMotionDeadzone;

	if (_hasPrevOwnerWorld)
	{
		if (Vec3::Distance(ownerPos, _prevOwnerPos) >= _desc.teleportDistance)
		{
			ResetPose(model);
			ownerMotionDelta = Vec3::Zero;
		}
	}

	Capture_CurrentPose(model, ownerWorld);
	if (isOwnerIdle)
	{
		const float driverMotionDeadzoneSq = _desc.idleMotionDeadzone * _desc.idleMotionDeadzone;
		for (const auto& chain : _chains)
		{
			if (chain.driverMotionDelta.LengthSquared() > driverMotionDeadzoneSq)
			{
				isOwnerIdle = false;
				break;
			}
		}
	}

	const ModelCom* collisionModel = _desc.collisionModel ? _desc.collisionModel : model;
	const Matrix collisionOwnerWorld = _desc.collisionOwnerWorldMatrix ? *_desc.collisionOwnerWorldMatrix : ownerWorld;

	if (Ensure_PhysXResources(model, collisionModel, ownerWorld, collisionOwnerWorld))
	{
		if (!Validate_RootAlignment(model, ownerWorld))
		{
			Reset_PhysXStateToPose(model, collisionModel, ownerWorld, collisionOwnerWorld);
		}

		Apply_AnchorFrameAdvection();
		Sync_KinematicActors(model, collisionModel, ownerWorld, collisionOwnerWorld);
		
		if (_desc.windScale > 0.001f && _desc.windDirection.LengthSquared() > 0.001f)
		{
			_windTime += std::max(0.f, timeDelta);
			Apply_WindForce(_desc.windDirection, _desc.windScale);
		}
		Apply_OwnerInertiaForce(ownerMotionDelta, timeDelta, isOwnerIdle);
		Apply_PoseSpringForces(timeDelta, isOwnerIdle);

		if (_scene)
		{
			_scene->simulate(timeDelta);
			_scene->fetchResults(true);
		}

		Apply_PoseRestoreAndDamping(timeDelta, isOwnerIdle);
		Update_SolvedPositionsFromActors();

		vector<WorldCapsule> worldCapsules = Build_WorldCapsules(collisionModel, collisionOwnerWorld);
		
		Apply_LengthAndBendConstraints();
		Apply_LocalOffsetConstraints(timeDelta, isOwnerIdle);
		Apply_PostSimulationConstraints(worldCapsules);

		if (_desc.writeBackEnabled)
			Write_BackToModel(model, ownerWorld, isOwnerIdle);

		Trace_BoneMotion(model, ownerWorld);
		Build_DebugOutput(model, ownerWorld);
	}

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
	if (model == nullptr) return;

	Restore_PoseLocals(model);
	Release_PhysXResources();
	_chains.clear();
	_chainsInitialized = false;
	_hasCapturedPose = false;
	_hasPrevOwnerWorld = false;
	_windTime = 0.f;
	Initialize_Chains(model);
	ResetPose(model);
}

void PhysicsModifier::RuntimeResetPose(ModelCom* model)
{
	if (model == nullptr) return;

	Restore_PoseLocals(model);
	ResetPose(model);
}

void PhysicsModifier::ResetPose(ModelCom* model)
{
	if (model == nullptr) return;
	
	model->Rebuild_BoneCombinedMatrices();
	if (!_chainsInitialized)
	{
		Initialize_Chains(model);
	}

	if (_chains.empty()) return;

	_desc.debugCollisionHitCount = 0;
	_desc.debugConstraintProjectionCount = 0;
	_desc.debugLocalClampCount = 0;
	_desc.debugLocalClampXCount = 0;
	_desc.debugLocalClampYCount = 0;
	_desc.debugLocalClampZCount = 0;
	_desc.debugInertiaMagnitude = 0.f;
	_desc.debugTraceMaxNodeMotion = 0.f;
	_desc.debugTraceMaxOutputAngle = 0.f;
	Matrix ownerWorld = Get_OwnerWorld(_desc);
	const ModelCom* collisionModel = _desc.collisionModel ? _desc.collisionModel : model;
	const Matrix collisionOwnerWorld = _desc.collisionOwnerWorldMatrix ? *_desc.collisionOwnerWorldMatrix : ownerWorld;
	Reset_PhysXStateToPose(model, collisionModel, ownerWorld, collisionOwnerWorld);
	_prevOwnerPos = Get_Position(ownerWorld);
	_hasPrevOwnerWorld = true;
	_windTime = 0.f;
}

void PhysicsModifier::Restore_PoseLocals(ModelCom* model)
{
	if (model == nullptr || _chains.empty()) return;

	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			model->Set_BoneLocalMatrix(
				node.boneIndex,
				node.bindLocal);
		}
	}
	model->Rebuild_BoneCombinedMatrices();
}


void PhysicsModifier::Apply_WindForce(const Vec3& direction, float scale)
{
	if (!_scene || direction.LengthSquared() <= 0.0001f || scale <= 0.0001f)
		return;

	Vec3 mainDirection = direction;
	mainDirection.Normalize();
	const float waveScale = std::max(0.f, _desc.windWaveScale);
	const float waveFrequency = std::max(0.f, _desc.windWaveFrequency);
	const float waveDepthPhase = _desc.windWaveDepthPhase;

	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			if (node.isKinematic || !node.actor)
				continue;
			const float depth = std::clamp(node.depthRatio, 0.f, 1.f);
			const float depthWeight = std::max(0.001f, SmoothStep(depth));
			float waveFactor = 1.f;
			if (waveScale > 0.0001f && waveFrequency > 0.0001f)
			{
				const float wavePhase = _windTime * XM_2PI * waveFrequency + depth * XM_2PI * waveDepthPhase;
				waveFactor = std::max(0.f, 1.f + sinf(wavePhase) * waveScale);
			}

			const Vec3 force = mainDirection * (scale * depthWeight * waveFactor);
			node.actor->addForce(To_PxVec3(force), PxForceMode::eFORCE, true);
			node.actor->wakeUp();
		}
	}
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
		Release_PhysX(blocker.actor);
	}
	_blockers.clear();

	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			Release_PhysX(node.joint);
			Release_PhysX(node.actor);
		}
		Release_PhysX(chain.anchorActor);
	}

	Release_PhysX(_material);
	
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

	_chains.clear();
	_chainsInitialized = true;
	for (const auto& desc : _desc.chains)
	{
		const int32 rootIndex = model->Get_BoneIndex(desc.rootBoneName);
		if (rootIndex < 0) continue;

		Chain chain{};
		chain.desc = desc;
		chain.anchorBoneIndex = model->Get_BoneParentIndex(static_cast<uint32>(rootIndex));
		
		BuildChainRecursive(model, rootIndex, -1, chain);

		if (chain.nodes.size() > 1)
		{
			if (_desc.useHierarchicalDepthRatio)
			{
				vector<uint32> nodeDepths(chain.nodes.size(), 0u);
				uint32 maxNodeDepth = 0u;
				for (size_t i = 0; i < chain.nodes.size(); ++i)
				{
					const int32 parentNodeIndex = chain.nodes[i].parentNodeIndex;
					if (parentNodeIndex >= 0 && static_cast<size_t>(parentNodeIndex) < nodeDepths.size())
						nodeDepths[i] = nodeDepths[static_cast<size_t>(parentNodeIndex)] + 1u;
					maxNodeDepth = std::max(maxNodeDepth, nodeDepths[i]);
				}
				for (size_t i = 0; i < chain.nodes.size(); ++i)
				{
					chain.nodes[i].depthRatio = maxNodeDepth > 0u
						? static_cast<float>(nodeDepths[i]) / static_cast<float>(maxNodeDepth)
						: 0.f;
				}
			}
			else
			{
				for (size_t i = 0; i < chain.nodes.size(); ++i)
				{
					chain.nodes[i].depthRatio = static_cast<float>(i) / static_cast<float>(chain.nodes.size() - 1);
				}
			}
			_chains.push_back(std::move(chain));
		}
	}
}

void PhysicsModifier::BuildChainRecursive(ModelCom* model, int32 boneIndex, int32 parentNodeIndex, Chain& outChain)
{
	if (model == nullptr || boneIndex < 0) return;

	Node node{};
	node.boneIndex = static_cast<uint32>(boneIndex);
	node.parentNodeIndex = parentNodeIndex;
	const Matrix bindLocal = model->Get_BoneLocalMatrix(static_cast<uint32>(boneIndex));
	node.bindLocal = bindLocal;
	node.poseLocal = bindLocal;
	node.bindLocalDir = Get_LocalDirection(Matrix::Identity, Get_Position(bindLocal));

	Vec3 bindScale = Vec3::One;
	Quat bindRotation = Quat::Identity;
	Vec3 bindTranslation = Vec3::Zero;
	Matrix bindLocalForDecompose = bindLocal;
	bindLocalForDecompose.Decompose(bindScale, bindRotation, bindTranslation);
	node.bindScale = bindScale;

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
	if (model == nullptr) return;

	for (const auto& chain : _chains)
	{
		for (const auto& node : chain.nodes)
		{
			model->Set_BoneLocalMatrix(node.boneIndex, node.bindLocal);
		}
	}

	model->Rebuild_BoneCombinedMatrices();

	for (auto& chain : _chains)
	{
		const Matrix previousAnchorWorld = chain.anchorWorld;
		const bool hadPrevAnchorWorld = chain.hasPrevAnchorWorld;

		chain.anchorWorld = ownerWorld;
		if (chain.anchorBoneIndex >= 0)
		{
			if (const Matrix* anchorCombined = model->Get_BoneMatrixPtr(static_cast<uint32>(chain.anchorBoneIndex)))
				chain.anchorWorld = (*anchorCombined) * ownerWorld;
		}
		chain.prevAnchorWorld = hadPrevAnchorWorld ? previousAnchorWorld : chain.anchorWorld;
		chain.hasPrevAnchorWorld = true;

		for (auto& node : chain.nodes)
		{
			node.poseLocal = model->Get_BoneLocalMatrix(node.boneIndex);
			const Matrix* boneCombined = model->Get_BoneMatrixPtr(node.boneIndex);
			if (boneCombined == nullptr)
				continue;

			node.originalWorld = (*boneCombined) * ownerWorld;

			Matrix parentWorld = chain.anchorWorld;
			if (node.parentNodeIndex >= 0)
			{
				parentWorld = chain.nodes[static_cast<size_t>(node.parentNodeIndex)].originalWorld;
			}

			const Vec3 nodePosition = Get_Position(node.originalWorld);
			const Vec3 parentPosition = Get_Position(parentWorld);
			const Vec3 poseWorldDir = nodePosition - parentPosition;
			const float poseLength = poseWorldDir.Length();
			node.length = poseLength;
			if (node.hasPrevPosePosition)
			{
				node.poseMotionDelta = nodePosition - node.prevPosePosition;
			}
			else
			{
				node.poseMotionDelta = Vec3::Zero;
				node.hasPrevPosePosition = true;
			}
			node.prevPosePosition = nodePosition;
			node.poseLocalDir = poseLength > 0.0001f
				? Get_LocalDirection(parentWorld, poseWorldDir)
				: node.bindLocalDir;
		}

		const Vec3 driverPosition = !chain.nodes.empty()
			? Get_Position(chain.nodes.front().originalWorld)
			: Get_Position(chain.anchorWorld);
		if (chain.hasPrevDriverPosition)
		{
			chain.driverMotionDelta = driverPosition - chain.prevDriverPosition;
		}
		else
		{
			chain.driverMotionDelta = Vec3::Zero;
			chain.hasPrevDriverPosition = true;
		}
		chain.prevDriverPosition = driverPosition;
	}

	_hasCapturedPose = !_chains.empty();
}

bool PhysicsModifier::Ensure_PhysXResources(ModelCom* model, const ModelCom* collisionModel, const Matrix& ownerWorld, const Matrix& collisionOwnerWorld)
{
	if (_physxResourcesReady) return true;

	_physics = GAME->Get_PhysXPhysics();
	if (!_physics) return false;

	PxSceneDesc sceneDesc(_physics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, -9.81f * std::max(0.f, _desc.gravityScale), 0.0f);
	PxDefaultCpuDispatcher* dispatcher = PxDefaultCpuDispatcherCreate(1);
	if (dispatcher == nullptr) return false;
	sceneDesc.cpuDispatcher = dispatcher;
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;
	_scene = _physics->createScene(sceneDesc);
	if (_scene == nullptr)
	{
		dispatcher->release();
		return false;
	}

	_material = _physics->createMaterial(0.2f, 0.2f, 0.0f);
	if (_material == nullptr)
	{
		Release_PhysXResources();
		return false;
	}

	for (auto& chain : _chains)
	{
		PxTransform anchorPose(To_PxVec3(Get_Position(chain.anchorWorld)), To_PxQuat(Extract_Rotation(chain.anchorWorld)));
		chain.anchorActor = _physics->createRigidDynamic(anchorPose);
		if (chain.anchorActor == nullptr)
		{
			Release_PhysXResources();
			return false;
		}
		chain.anchorActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
		chain.anchorActor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);
		_scene->addActor(*chain.anchorActor);

		for (size_t i = 0; i < chain.nodes.size(); ++i)
		{
			auto& node = chain.nodes[i];
			node.isKinematic = (node.parentNodeIndex < 0);
			
			PxTransform nodePose(To_PxVec3(Get_Position(node.originalWorld)), To_PxQuat(Extract_Rotation(node.originalWorld)));
			node.actor = _physics->createRigidDynamic(nodePose);
			if (node.actor == nullptr)
			{
				Release_PhysXResources();
				return false;
			}
			
			if (node.isKinematic)
			{
				node.actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
				node.actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);
			}
			else
			{
				float chainRadiusMul = chain.desc.radiusMul;
				node.debugSphereRadius = max(0.001f, _desc.collisionSphereRadius * chainRadiusMul);
				
				PxShape* shape = _physics->createShape(PxSphereGeometry(node.debugSphereRadius), *_material, true);
				if (shape == nullptr)
				{
					Release_PhysX(node.actor);
					Release_PhysXResources();
					return false;
				}

				Configure_ShapeFilter(shape, Collision_Channel::Monster_Attack, 0, ETOI(Collision_Channel::Monster_Body));
				if (!node.actor->attachShape(*shape))
				{
					shape->release();
					Release_PhysX(node.actor);
					Release_PhysXResources();
					return false;
				}
				shape->release();

				node.actor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
				node.actor->setLinearDamping(std::max(0.f, _desc.releaseInertiaDamping) * chain.desc.dampingMul);
				node.actor->setAngularDamping(std::max(0.f, _desc.releaseInertiaDamping) * chain.desc.dampingMul);
				node.actor->setSolverIterationCounts(std::max(1u, _desc.solverIterations), std::max(1u, _desc.solverIterations));
				node.actor->setSleepThreshold(0.0001f);
				if (_desc.maxMovementInertiaSpeed > 0.f)
					node.actor->setMaxLinearVelocity(_desc.maxMovementInertiaSpeed);
				PxRigidBodyExt::updateMassAndInertia(*node.actor, max(0.001f, node.debugSphereRadius));
			}
			_scene->addActor(*node.actor);

			if (!node.isKinematic)
			{
				PxRigidDynamic* parentActor = (node.parentNodeIndex >= 0) ? chain.nodes[node.parentNodeIndex].actor : chain.anchorActor;
				if (parentActor == nullptr)
				{
					Release_PhysXResources();
					return false;
				}

				const PxTransform parentPose = parentActor->getGlobalPose();
				const PxTransform childPose = node.actor->getGlobalPose();
				PxVec3 jointAxis = childPose.p - parentPose.p;
				if (jointAxis.magnitudeSquared() <= 0.000001f)
					jointAxis = PxVec3(1.f, 0.f, 0.f);
				else
					jointAxis.normalize();

				Vec3 referenceUp = Vec3::TransformNormal(Vec3::UnitY, Matrix::CreateFromQuaternion(Extract_Rotation(node.originalWorld)));
				if (node.parentNodeIndex >= 0)
				{
					referenceUp = Vec3::TransformNormal(
						Vec3::UnitY,
						Matrix::CreateFromQuaternion(Extract_Rotation(chain.nodes[static_cast<size_t>(node.parentNodeIndex)].originalWorld)));
				}
				else
				{
					referenceUp = Vec3::TransformNormal(Vec3::UnitY, Matrix::CreateFromQuaternion(Extract_Rotation(chain.anchorWorld)));
				}
				if (referenceUp.LengthSquared() <= 0.000001f)
					referenceUp = Vec3::UnitY;
				referenceUp.Normalize();

				const PxTransform jointWorldFrame(parentPose.p, Build_JointFrameQuat(From_PxVec3(jointAxis), referenceUp));
				const PxTransform parentLocalFrame = parentPose.transformInv(jointWorldFrame);
				const PxTransform childLocalFrame = childPose.transformInv(jointWorldFrame);

				node.joint = PxSphericalJointCreate(*_physics, parentActor, parentLocalFrame, node.actor, childLocalFrame);
				if (node.joint == nullptr)
				{
					Release_PhysXResources();
					return false;
				}
				
				if (_desc.enableJointLimit)
				{
					float maxAngle = Compute_NodeAngleLimitDegree(chain, node.depthRatio);
					float swingLimitDegree = std::clamp(maxAngle, 0.f, 180.f);
					float swingLimit = XMConvertToRadians(swingLimitDegree);

					PxJointLimitCone limitCone(swingLimit, swingLimit);
					limitCone.restitution = 0.f;
					limitCone.stiffness = 0.f;
					limitCone.damping = 0.f;

					static_cast<PxSphericalJoint*>(node.joint)->setLimitCone(limitCone);
					static_cast<PxSphericalJoint*>(node.joint)->setSphericalJointFlag(PxSphericalJointFlag::eLIMIT_ENABLED, true);
				}
				node.joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, false);

				const bool parentIsDynamicNode =
					node.parentNodeIndex >= 0 &&
					!chain.nodes[static_cast<size_t>(node.parentNodeIndex)].isKinematic;
				const float parentConstraintScale = parentIsDynamicNode ? 1.0f : 0.0f;
				node.joint->setInvMassScale0(parentConstraintScale);
				node.joint->setInvInertiaScale0(parentConstraintScale);
				node.joint->setInvMassScale1(1.0f);
				node.joint->setInvInertiaScale1(1.0f);
			}
		}
	}

	vector<WorldCapsule> caps = Build_WorldCapsules(collisionModel, collisionOwnerWorld);
	for (size_t i = 0; i < caps.size(); ++i)
	{
		Blocker blocker{};
		const float blockerRadius = caps[i].radius;
		const float blockerHalfHeight = Vec3::Distance(caps[i].startPoint, caps[i].endPoint) * 0.5f;
		const bool blockerIsSphere = blockerHalfHeight <= 0.0001f;
		
		Vec3 center = (caps[i].startPoint + caps[i].endPoint) * 0.5f;
		Matrix frame;
		if (!Build_SegmentWorldFrame(caps[i].startPoint, caps[i].endPoint, frame))
			frame = Matrix::CreateTranslation(center);

		PxTransform blockerPose(To_PxVec3(center), To_PxQuat(Extract_Rotation(frame)));
		blocker.actor = _physics->createRigidDynamic(blockerPose);
		if (blocker.actor == nullptr)
		{
			Release_PhysXResources();
			return false;
		}
		blocker.actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
		blocker.actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);

		PxGeometry* geom = nullptr;
		PxSphereGeometry sphereGeom(blockerRadius);
		PxCapsuleGeometry capGeom(blockerRadius, blockerHalfHeight);
		
		if (blockerIsSphere) geom = &sphereGeom;
		else geom = &capGeom;

		PxShape* shape = PxRigidActorExt::createExclusiveShape(*blocker.actor, *geom, *_material);
		if (shape == nullptr)
		{
			Release_PhysX(blocker.actor);
			Release_PhysXResources();
			return false;
		}
		Configure_ShapeFilter(shape, Collision_Channel::Monster_Body, 0, ETOI(Collision_Channel::Monster_Attack));
		
		_scene->addActor(*blocker.actor);
		_blockers.push_back(blocker);
	}

	_physxResourcesReady = true;
	Reset_PhysXStateToPose(model, collisionModel, ownerWorld, collisionOwnerWorld);
	return true;
}

void PhysicsModifier::Reset_PhysXStateToPose(ModelCom* model, const ModelCom* collisionModel, const Matrix& ownerWorld, const Matrix& collisionOwnerWorld)
{
	if (model == nullptr) return;

	Restore_PoseLocals(model);
	Capture_CurrentPose(model, ownerWorld);
	Sync_KinematicActors(model, collisionModel, ownerWorld, collisionOwnerWorld);
	
	for (auto& chain : _chains)
	{
		const Vec3 driverPosition = !chain.nodes.empty()
			? Get_Position(chain.nodes.front().originalWorld)
			: Get_Position(chain.anchorWorld);
		chain.prevDriverPosition = driverPosition;
		chain.driverMotionDelta = Vec3::Zero;
		chain.prevDriverVelocity = Vec3::Zero;
		chain.hasPrevDriverPosition = true;
		chain.hasPrevDriverVelocity = false;
		chain.prevAnchorWorld = chain.anchorWorld;
		chain.hasPrevAnchorWorld = true;
		chain.smoothedLateralAxis = Vec3::UnitX;
		chain.hasSmoothedLateralAxis = false;

		if (chain.anchorActor)
		{
			const PxTransform anchorPose(To_PxVec3(Get_Position(chain.anchorWorld)), To_PxQuat(Extract_Rotation(chain.anchorWorld)));
			chain.anchorActor->setGlobalPose(anchorPose);
			chain.anchorActor->setKinematicTarget(anchorPose);
		}

		for (auto& node : chain.nodes)
		{
			const Vec3 posePosition = Get_Position(node.originalWorld);
			const Quat poseWorldRotation = Extract_Rotation(node.originalWorld);
			const PxTransform pose(To_PxVec3(posePosition), To_PxQuat(poseWorldRotation));

			if (node.actor)
			{
				node.actor->setGlobalPose(pose);
				if (node.isKinematic)
				{
					node.actor->setKinematicTarget(pose);
				}
				else
				{
					node.actor->setLinearVelocity(PxVec3(0.f));
					node.actor->setAngularVelocity(PxVec3(0.f));
					node.actor->clearForce();
					node.actor->clearTorque();
				}
			}

			node.solvedPosition = posePosition;
			node.prevSolvedPosition = posePosition;
			node.prevPosePosition = posePosition;
			node.poseMotionDelta = Vec3::Zero;
			node.hasPrevPosePosition = true;
			node.smoothedInertiaVelocity = Vec3::Zero;
			node.hasSmoothedInertiaVelocity = false;

			Vec3 poseScale = Vec3::One;
			Quat poseRotation = Quat::Identity;
			Vec3 poseTranslation = Vec3::Zero;
			Matrix poseLocal = node.poseLocal;
			poseLocal.Decompose(poseScale, poseRotation, poseTranslation);
			poseRotation.Normalize();
			node.bindScale = poseScale;
			node.smoothedLocalDirection = node.poseLocalDir.LengthSquared() > 0.0001f
				? node.poseLocalDir
				: node.bindLocalDir;
			if (node.smoothedLocalDirection.LengthSquared() > 0.0001f)
			{
				node.smoothedLocalDirection.Normalize();
				node.hasSmoothedLocalDirection = true;
			}
			else
			{
				node.smoothedLocalDirection = Vec3::Zero;
				node.hasSmoothedLocalDirection = false;
			}
			node.smoothedLocalTranslation = poseTranslation;
			node.smoothedLocalRotation = poseRotation;
			node.hasSmoothedLocalTransform = true;

			node.collisionOutputPosition = Vec3::Zero;
			node.hasCollisionOutputPosition = false;
			node.hadContactThisFrame = false;
			node.hadPushThisFrame = false;
		}
	}

	vector<WorldCapsule> caps = Build_WorldCapsules(collisionModel, collisionOwnerWorld);
	for (size_t i = 0; i < min(caps.size(), _blockers.size()); ++i)
	{
		if (_blockers[i].actor == nullptr)
			continue;

		Vec3 center = (caps[i].startPoint + caps[i].endPoint) * 0.5f;
		Matrix frame;
		if (!Build_SegmentWorldFrame(caps[i].startPoint, caps[i].endPoint, frame))
			frame = Matrix::CreateTranslation(center);

		const PxTransform blockerPose(To_PxVec3(center), To_PxQuat(Extract_Rotation(frame)));
		_blockers[i].actor->setGlobalPose(blockerPose);
		_blockers[i].actor->setKinematicTarget(blockerPose);
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
			node.collisionOutputPosition = Vec3::Zero;
			node.hasCollisionOutputPosition = false;
			node.hadContactThisFrame = false;
			node.hadPushThisFrame = false;
			if (node.isKinematic && node.actor)
			{
				PxTransform nodePose(To_PxVec3(Get_Position(node.originalWorld)), To_PxQuat(Extract_Rotation(node.originalWorld)));
				node.actor->setKinematicTarget(nodePose);
				node.actor->wakeUp();
			}
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

void PhysicsModifier::Apply_AnchorFrameAdvection()
{
	if (!_desc.advectSimulationWithAnchor)
		return;

	const float alpha = std::clamp(_desc.anchorAdvectionAlpha, 0.f, 1.f);
	if (alpha <= 0.0001f)
		return;

	for (auto& chain : _chains)
	{
		if (!chain.hasPrevAnchorWorld)
			continue;

		const Vec3 prevAnchorPosition = Get_Position(chain.prevAnchorWorld);
		const Vec3 anchorPosition = Get_Position(chain.anchorWorld);
		const Vec3 anchorDelta = anchorPosition - prevAnchorPosition;
		if (anchorDelta.LengthSquared() <= 0.000001f)
		{
			const Quat prevRotation = Extract_Rotation(chain.prevAnchorWorld);
			const Quat currentRotation = Extract_Rotation(chain.anchorWorld);
			if (fabsf(prevRotation.Dot(currentRotation)) > 0.99995f)
				continue;
		}

		const Matrix prevAnchorInv = chain.prevAnchorWorld.Invert();
		const Matrix frameDelta = prevAnchorInv * chain.anchorWorld;

		for (auto& node : chain.nodes)
		{
			if (node.isKinematic || node.actor == nullptr)
				continue;

			PxTransform pose = node.actor->getGlobalPose();
			const Vec3 currentPosition = From_PxVec3(pose.p);
			const Vec3 anchorLocalPosition = Vec3::Transform(currentPosition, prevAnchorInv);
			Vec3 advectedPosition = Vec3::Transform(anchorLocalPosition, chain.anchorWorld);
			if (alpha < 0.999f)
				advectedPosition = Vec3::Lerp(currentPosition, advectedPosition, alpha);

			pose.p = To_PxVec3(advectedPosition);
			node.actor->setGlobalPose(pose);

			const Vec3 currentVelocity = From_PxVec3(node.actor->getLinearVelocity());
			Vec3 advectedVelocity = Vec3::TransformNormal(currentVelocity, frameDelta);
			if (alpha < 0.999f)
				advectedVelocity = Vec3::Lerp(currentVelocity, advectedVelocity, alpha);
			node.actor->setLinearVelocity(To_PxVec3(advectedVelocity));

			const Vec3 currentAngularVelocity = From_PxVec3(node.actor->getAngularVelocity());
			Vec3 advectedAngularVelocity = Vec3::TransformNormal(currentAngularVelocity, frameDelta);
			if (alpha < 0.999f)
				advectedAngularVelocity = Vec3::Lerp(currentAngularVelocity, advectedAngularVelocity, alpha);
			node.actor->setAngularVelocity(To_PxVec3(advectedAngularVelocity));
			node.actor->wakeUp();
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

void PhysicsModifier::Apply_OwnerInertiaForce(const Vec3& ownerMotionDelta, float timeDelta, bool isOwnerIdle)
{
	const float inertiaScale = std::max(0.f, _desc.movementInertiaScale);
	if (timeDelta <= 0.0001f || inertiaScale <= 0.0001f)
		return;

	_desc.debugInertiaMagnitude = 0.f;
	if (isOwnerIdle)
	{
		for (auto& chain : _chains)
		{
			chain.prevDriverVelocity = Vec3::Zero;
			chain.hasPrevDriverVelocity = false;
			for (auto& node : chain.nodes)
			{
				node.smoothedInertiaVelocity = Vec3::Zero;
				node.hasSmoothedInertiaVelocity = false;
			}
		}
		return;
	}

	const float maxSpeed = std::max(0.f, _desc.maxMovementInertiaSpeed);
	const float lateralScale = std::max(0.f, _desc.lateralInertiaScale);
	const float verticalScale = std::max(0.f, _desc.verticalInertiaScale);
	const float depthScale = std::max(0.f, _desc.depthInertiaScale);
	const float ownerRotationInertiaScale = std::max(0.f, _desc.ownerRotationInertiaScale);
	const bool includeDriverLateralInertia = _desc.includeDriverLateralInertia;
	const bool useSteadyInertia = _desc.useSteadyInertia;
	const float accelerationScale = std::max(0.f, _desc.accelerationInertiaScale);
	const float velocitySmoothAlpha = std::clamp(_desc.inertiaVelocitySmoothAlpha, 0.f, 1.f);
	const float releaseVelocityAlpha = std::clamp(_desc.releaseInertiaDamping, 0.f, 0.98f);

	for (auto& chain : _chains)
	{
		Vec3 poseDriverDelta = chain.driverMotionDelta;
		if (poseDriverDelta.LengthSquared() <= 0.000001f)
			poseDriverDelta = ownerMotionDelta;

		const Vec3 inertiaDriverDelta = poseDriverDelta;

		const Vec3 rawDriverVelocity = inertiaDriverDelta / timeDelta;
		Vec3 driverVelocity = rawDriverVelocity;
		Vec3 driverVelocityDelta = Vec3::Zero;
		if (chain.hasPrevDriverVelocity)
		{
			const float driverFollowAlpha = rawDriverVelocity.LengthSquared() < chain.prevDriverVelocity.LengthSquared()
				? std::max(velocitySmoothAlpha, releaseVelocityAlpha)
				: velocitySmoothAlpha;
			driverVelocity = Vec3::Lerp(chain.prevDriverVelocity, rawDriverVelocity, driverFollowAlpha);
			driverVelocityDelta = driverVelocity - chain.prevDriverVelocity;
		}
		chain.prevDriverVelocity = driverVelocity;
		chain.hasPrevDriverVelocity = true;

		const Vec3 rootPosition = !chain.nodes.empty()
			? Get_Position(chain.nodes.front().originalWorld)
			: Get_Position(chain.anchorWorld);
		const Vec3 tipPosition = !chain.nodes.empty()
			? Get_Position(chain.nodes.back().originalWorld)
			: rootPosition + Vec3::UnitZ;

		Vec3 chainAxis = tipPosition - rootPosition;
		if (chainAxis.LengthSquared() <= 0.000001f)
			chainAxis = Vec3::TransformNormal(Vec3::UnitZ, chain.anchorWorld);
		if (chainAxis.LengthSquared() <= 0.000001f)
			chainAxis = Vec3::UnitZ;
		chainAxis.Normalize();

		Vec3 upAxis = Vec3::TransformNormal(Vec3::UnitY, chain.anchorWorld);
		if (upAxis.LengthSquared() <= 0.000001f)
			upAxis = Vec3::UnitY;
		upAxis.Normalize();

		Vec3 lateralAxis = Vec3::TransformNormal(Vec3::UnitX, chain.anchorWorld);
		lateralAxis -= chainAxis * lateralAxis.Dot(chainAxis);
		if (lateralAxis.LengthSquared() <= 0.000001f)
			lateralAxis = upAxis.Cross(chainAxis);
		if (lateralAxis.LengthSquared() <= 0.000001f)
			lateralAxis = Vec3::UnitX;
		lateralAxis.Normalize();
		if (chain.hasSmoothedLateralAxis)
		{
			if (chain.smoothedLateralAxis.Dot(lateralAxis) < 0.f)
				lateralAxis = -lateralAxis;

			const float lateralAxisFollowAlpha = std::clamp(_desc.outputSmoothAlpha, 0.f, 1.f);
			if (lateralAxisFollowAlpha > 0.f)
			{
				lateralAxis = Vec3::Lerp(chain.smoothedLateralAxis, lateralAxis, lateralAxisFollowAlpha);
				if (lateralAxis.LengthSquared() <= 0.000001f)
					lateralAxis = chain.smoothedLateralAxis;
				else
					lateralAxis.Normalize();
			}
		}
		chain.smoothedLateralAxis = lateralAxis;
		chain.hasSmoothedLateralAxis = true;

		Vec3 depthAxis = Vec3::TransformNormal(Vec3::UnitZ, chain.anchorWorld);
		depthAxis -= chainAxis * depthAxis.Dot(chainAxis);
		depthAxis -= lateralAxis * depthAxis.Dot(lateralAxis);
		if (depthAxis.LengthSquared() <= 0.000001f)
			depthAxis = chainAxis.Cross(lateralAxis);
		if (depthAxis.LengthSquared() <= 0.000001f)
			depthAxis = Vec3::UnitZ;
		depthAxis.Normalize();

		Vec3 verticalAxis = upAxis;
		verticalAxis -= chainAxis * verticalAxis.Dot(chainAxis);
		if (verticalAxis.LengthSquared() > 0.000001f)
			verticalAxis.Normalize();

		auto buildWeightedTangentialVelocity =
			[&](const Vec3& worldVelocity, bool includeLateral) -> Vec3
			{
				Vec3 weightedVelocity =
					depthAxis * (worldVelocity.Dot(depthAxis) * depthScale);
				if (includeLateral)
					weightedVelocity += lateralAxis * (worldVelocity.Dot(lateralAxis) * lateralScale);
				if (verticalAxis.LengthSquared() > 0.000001f)
					weightedVelocity += verticalAxis * (worldVelocity.Dot(verticalAxis) * verticalScale);
				weightedVelocity -= chainAxis * weightedVelocity.Dot(chainAxis);
				return weightedVelocity;
		};

		const Vec3 driverTangentialVelocity = buildWeightedTangentialVelocity(driverVelocity, includeDriverLateralInertia);
		const Vec3 driverTangentialVelocityDelta = buildWeightedTangentialVelocity(driverVelocityDelta, true);

		for (auto& node : chain.nodes)
		{
			if (node.isKinematic || node.actor == nullptr)
				continue;

			const float depth = std::clamp(node.depthRatio, 0.f, 1.f);
			const float tipRatio = SmoothStep(depth);
			const float tipBias = tipRatio;
			const float tipWeight = 1.f + (std::max(0.f, _desc.tipMovementInertiaScale) - 1.f) * tipBias;
			auto releaseSmoothedInertiaVelocity =
				[releaseVelocityAlpha](Node& targetNode)
				{
					if (!targetNode.hasSmoothedInertiaVelocity)
						return;

					targetNode.smoothedInertiaVelocity = Vec3::Lerp(
						targetNode.smoothedInertiaVelocity,
						Vec3::Zero,
						releaseVelocityAlpha);
					if (targetNode.smoothedInertiaVelocity.LengthSquared() <= 0.000001f)
					{
						targetNode.smoothedInertiaVelocity = Vec3::Zero;
						targetNode.hasSmoothedInertiaVelocity = false;
					}
				};
			const Vec3 relativePoseDelta = node.poseMotionDelta - poseDriverDelta;
			const Vec3 relativePoseVelocity = relativePoseDelta / timeDelta;
			const Vec3 relativeTangentialVelocity = buildWeightedTangentialVelocity(relativePoseVelocity, true);
			Vec3 ownerRotationTangentialVelocity = Vec3::Zero;
			if (ownerRotationInertiaScale > 0.0001f && chain.hasPrevAnchorWorld)
			{
				const Vec3 restPosition = Get_Position(node.originalWorld);
				const Matrix anchorInv = chain.anchorWorld.Invert();
				const Vec3 anchorLocalPosition = Vec3::Transform(restPosition, anchorInv);
				const Vec3 previousAnchorDrivenPosition = Vec3::Transform(anchorLocalPosition, chain.prevAnchorWorld);
				const Vec3 anchorDrivenDelta = restPosition - previousAnchorDrivenPosition;
				const Vec3 rotationDelta = anchorDrivenDelta - poseDriverDelta;
				ownerRotationTangentialVelocity = buildWeightedTangentialVelocity(rotationDelta / timeDelta, true) * ownerRotationInertiaScale;
			}
			const Vec3 steadyTangentialVelocity = useSteadyInertia
				? driverTangentialVelocity + ownerRotationTangentialVelocity
				: Vec3::Zero;
			const Vec3 tangentialVelocity = steadyTangentialVelocity + relativeTangentialVelocity;
			const Vec3 tangentialVelocityDelta = driverTangentialVelocityDelta;
			const Vec3 inertiaVelocity = tangentialVelocity + tangentialVelocityDelta * accelerationScale;

			if (inertiaVelocity.LengthSquared() <= 0.000001f)
			{
				releaseSmoothedInertiaVelocity(node);
				continue;
			}

			Vec3 targetVelocity =
				-inertiaVelocity *
				(inertiaScale * tipWeight);

			if (targetVelocity.LengthSquared() <= 0.000001f)
			{
				releaseSmoothedInertiaVelocity(node);
				continue;
			}

			const float finalMaxSpeed = maxSpeed;
			const float targetVelocityLength = targetVelocity.Length();
			if (finalMaxSpeed > 0.0001f && targetVelocityLength > finalMaxSpeed)
			{
				targetVelocity /= targetVelocityLength;
				targetVelocity *= finalMaxSpeed;
			}

			if (node.hasSmoothedInertiaVelocity)
			{
				const float targetFollowAlpha = targetVelocity.LengthSquared() < node.smoothedInertiaVelocity.LengthSquared()
					? std::max(std::max(velocitySmoothAlpha, _desc.outputSmoothAlpha), releaseVelocityAlpha)
					: std::max(velocitySmoothAlpha, _desc.outputSmoothAlpha);
				targetVelocity = Vec3::Lerp(
					node.smoothedInertiaVelocity,
					targetVelocity,
					std::clamp(targetFollowAlpha, 0.f, 1.f));
			}
			node.smoothedInertiaVelocity = targetVelocity;
			node.hasSmoothedInertiaVelocity = true;

			Vec3 velocityChange = targetVelocity;
			const float smoothedTargetLength = targetVelocity.Length();
			if (smoothedTargetLength > 0.000001f)
			{
				Vec3 targetDirection = targetVelocity / smoothedTargetLength;
				const Vec3 currentVelocity = From_PxVec3(node.actor->getLinearVelocity());
				const float currentAlongTarget = currentVelocity.Dot(targetDirection);
				if (currentAlongTarget > 0.f)
					velocityChange -= targetDirection * std::min(currentAlongTarget, smoothedTargetLength);
			}

			if (velocityChange.LengthSquared() <= 0.000001f)
				continue;

			_desc.debugInertiaMagnitude = std::max(_desc.debugInertiaMagnitude, velocityChange.Length());

			node.actor->addForce(To_PxVec3(velocityChange), PxForceMode::eVELOCITY_CHANGE);
			node.actor->wakeUp();
		}
	}
}

void PhysicsModifier::Apply_PoseSpringForces(float timeDelta, bool isOwnerIdle)
{
	if (timeDelta <= 0.0001f)
		return;

	const bool windActive =
		_desc.windScale > 0.001f &&
		_desc.windDirection.LengthSquared() > 0.001f;
	if (isOwnerIdle && !windActive)
		return;

	const float springStrength = std::max(0.f, _desc.poseSpringStrength);
	const float springDamping = std::clamp(_desc.poseSpringDamping, 0.f, 1.f);
	if (springStrength <= 0.0001f && springDamping <= 0.0001f)
		return;

	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			if (node.isKinematic || node.actor == nullptr)
				continue;

			const PxTransform actorPose = node.actor->getGlobalPose();
			const Vec3 currentPosition = From_PxVec3(actorPose.p);
			Matrix restParentWorld = chain.anchorWorld;
			Vec3 parentPosition = chain.anchorActor != nullptr
				? From_PxVec3(chain.anchorActor->getGlobalPose().p)
				: Get_Position(chain.anchorWorld);
			if (node.parentNodeIndex >= 0)
			{
				const Node& parentNode = chain.nodes[static_cast<size_t>(node.parentNodeIndex)];
				restParentWorld = parentNode.originalWorld;
				parentPosition = parentNode.actor != nullptr
					? From_PxVec3(parentNode.actor->getGlobalPose().p)
					: parentNode.solvedPosition;
			}
			Vec3 restWorldDir = Vec3::Zero;
			const Vec3 restLocalDir = node.poseLocalDir.LengthSquared() > 0.0001f
				? node.poseLocalDir
				: node.bindLocalDir;
			if (restLocalDir.LengthSquared() > 0.0001f)
			{
				restWorldDir = Vec3::TransformNormal(restLocalDir, restParentWorld);
				if (restWorldDir.LengthSquared() > 0.0001f)
					restWorldDir.Normalize();
			}
			const Vec3 restPosition = restWorldDir.LengthSquared() > 0.0001f
				? parentPosition + restWorldDir * node.length
				: Get_Position(node.originalWorld);
			const Vec3 linearVelocity = From_PxVec3(node.actor->getLinearVelocity());

			Vec3 velocityChange = Vec3::Zero;
			if (springStrength > 0.0001f)
			{
				const float restoreWeight = std::max(0.f, chain.desc.restoreMul);
				velocityChange += (restPosition - currentPosition) * springStrength * restoreWeight;
			}
			if (springDamping > 0.0001f)
			{
				velocityChange -= linearVelocity * springDamping;
			}

			const float maxSpringSpeed = std::max(0.f, _desc.poseRestoreMaxSpeed);
			const float velocityChangeLength = velocityChange.Length();
			if (maxSpringSpeed > 0.0001f && velocityChangeLength > maxSpringSpeed)
			{
				velocityChange /= velocityChangeLength;
				velocityChange *= maxSpringSpeed;
			}

			if (velocityChange.LengthSquared() <= 0.000001f)
				continue;

			node.actor->addForce(To_PxVec3(velocityChange), PxForceMode::eVELOCITY_CHANGE);
			node.actor->wakeUp();
		}
	}
}

void PhysicsModifier::Apply_LengthAndBendConstraints()
{
	const float lengthAlpha = std::clamp(_desc.lengthConstraintAlpha, 0.f, 1.f);
	const float bendAlpha = std::clamp(_desc.bendConstraintAlpha, 0.f, 1.f);
	if (lengthAlpha <= 0.0001f && bendAlpha <= 0.0001f)
		return;

	const uint32 iterations = std::max<uint32>(_desc.projectionIterations, 1u);

	for (uint32 iter = 0; iter < iterations; ++iter)
	{
		for (auto& chain : _chains)
		{
			for (auto& node : chain.nodes)
			{
				if (node.isKinematic || node.parentNodeIndex < 0 || node.length <= 0.0001f)
					continue;

				Node& parentNode = chain.nodes[static_cast<size_t>(node.parentNodeIndex)];
				const Vec3 parentPosition = parentNode.solvedPosition;
				Vec3 projectedPosition = node.solvedPosition;

				if (lengthAlpha > 0.0001f)
				{
					Vec3 parentToNode = projectedPosition - parentPosition;
					const float currentLength = parentToNode.Length();
					if (currentLength > 0.0001f)
					{
						const Vec3 lengthTarget = parentPosition + (parentToNode / currentLength) * node.length;
						projectedPosition = Vec3::Lerp(projectedPosition, lengthTarget, lengthAlpha);
					}
				}

				if (bendAlpha > 0.0001f)
				{
					Vec3 parentToNode = projectedPosition - parentPosition;
					if (parentToNode.LengthSquared() > 0.0001f)
					{
						parentToNode.Normalize();

						const Vec3 restLocalDir =
							node.poseLocalDir.LengthSquared() > 0.0001f
							? node.poseLocalDir
							: node.bindLocalDir;

						if (restLocalDir.LengthSquared() > 0.0001f)
						{
							const Matrix& parentWorld = parentNode.originalWorld;
							const Vec3 simulatedLocalDir = Get_LocalDirection(parentWorld, parentToNode);
							const float maxAngle = Compute_NodeAngleLimitDegree(chain, node.depthRatio);
							const Vec3 limitedLocalDir = Limit_LocalDirectionByAngle(restLocalDir, simulatedLocalDir, maxAngle);
							Vec3 limitedWorldDir = Vec3::TransformNormal(limitedLocalDir, parentWorld);
							if (limitedWorldDir.LengthSquared() > 0.0001f)
							{
								limitedWorldDir.Normalize();
								const Vec3 bendTarget = parentPosition + limitedWorldDir * node.length;
								projectedPosition = Vec3::Lerp(projectedPosition, bendTarget, bendAlpha);
							}
						}
					}
				}

				if ((projectedPosition - node.solvedPosition).LengthSquared() <= 0.000001f)
					continue;

				node.solvedPosition = projectedPosition;
				++_desc.debugConstraintProjectionCount;

				if (node.actor == nullptr)
					continue;

				PxTransform pose = node.actor->getGlobalPose();
				pose.p = To_PxVec3(projectedPosition);
				node.actor->setGlobalPose(pose);

				Vec3 segmentDirection = projectedPosition - parentPosition;
				if (segmentDirection.LengthSquared() > 0.000001f)
				{
					segmentDirection.Normalize();
					Vec3 linearVelocity = From_PxVec3(node.actor->getLinearVelocity());
					const Vec3 radialVelocity = segmentDirection * linearVelocity.Dot(segmentDirection);
					const Vec3 tangentVelocity = linearVelocity - radialVelocity;
					const float radialKeep = 1.f - std::clamp(
						lengthAlpha +
						bendAlpha,
						0.f,
						1.f);
					linearVelocity = tangentVelocity + radialVelocity * radialKeep;
					node.actor->setLinearVelocity(To_PxVec3(linearVelocity));
				}
			}
		}
	}
}

void PhysicsModifier::Apply_PostSimulationConstraints(const vector<WorldCapsule>& worldCapsules)
{
	if (!_desc.enableCollision || worldCapsules.empty()) return;

	const float skinWidth = std::max(0.f, _desc.collisionContactSkinWidth);
	const bool useHardCollisionProjection = _desc.useHardCollisionProjection;
	const uint32 iterations = useHardCollisionProjection
		? std::max(1u, _desc.collisionProjectionIterations)
		: _desc.collisionProjectionIterations;
	const uint32 hardProjectionIterations = useHardCollisionProjection
		? std::max(1u, _desc.hardCollisionProjectionIterations)
		: 0u;
	const float hardLengthAlpha = std::clamp(_desc.hardCollisionLengthAlpha, 0.f, 1.f);
	constexpr float kHardSegmentMinChildT = 0.35f;

	for (auto& chain : _chains)
	{
		for (uint32 iter = 0; iter < iterations; ++iter)
		{
			for (size_t i = 0; i < chain.nodes.size(); ++i)
			{
				auto& node = chain.nodes[i];
				if (node.isKinematic) continue;

				const float nodeRadius = std::max(0.02f, node.debugSphereRadius);
				const Vec3 currentPosition = node.solvedPosition;
				Vec3 position = currentPosition;
				bool resolved = false;

				Vec3 parentPosition = currentPosition;
				if (node.parentNodeIndex >= 0)
				{
					const Node& parentNode = chain.nodes[static_cast<size_t>(node.parentNodeIndex)];
					parentPosition = parentNode.actor != nullptr
						? From_PxVec3(parentNode.actor->getGlobalPose().p)
						: parentNode.solvedPosition;
				}

				auto pushPositionOut = [&](const Vec3& rawNormal, float penetration)
					{
						if (penetration <= 0.f)
							return;

						Vec3 normal = rawNormal;
						if (normal.LengthSquared() <= 0.0001f)
							normal = position - parentPosition;
						if (normal.LengthSquared() <= 0.0001f)
							normal = Get_Position(node.originalWorld) - parentPosition;
						if (normal.LengthSquared() <= 0.0001f)
							normal = Vec3::UnitY;
						normal.Normalize();

						const float depenetration = penetration + 0.0005f;
						position += normal * depenetration;
						resolved = true;
					};

				for (const auto& capsule : worldCapsules)
				{
					const float combinedRadius = nodeRadius + skinWidth + capsule.radius;
					const Vec3 closest = Closest_PointOnSegment(position, capsule.startPoint, capsule.endPoint);
					const Vec3 delta = position - closest;
					const float dist = delta.Length();

					if (dist < combinedRadius)
					{
						const Vec3 normal = dist > 0.0001f ? delta / dist : Vec3::Zero;
						pushPositionOut(
							normal,
							combinedRadius - dist);
					}

					if ((node.prevSolvedPosition - currentPosition).LengthSquared() > 0.000001f)
					{
						Vec3 sweepClosest = Vec3::Zero;
						Vec3 capsuleClosest = Vec3::Zero;
						float sweepT = 0.f;
						float capsuleT = 0.f;
						const float distanceSq = SegmentSegmentClosestPoints(
							node.prevSolvedPosition,
							currentPosition,
							capsule.startPoint,
							capsule.endPoint,
							sweepClosest,
							capsuleClosest,
							sweepT,
							capsuleT);
						const float combinedRadiusSq = combinedRadius * combinedRadius;
						if (distanceSq < combinedRadiusSq)
						{
							const float sweepDistance = sqrtf(std::max(0.f, distanceSq));
							Vec3 sweepNormal = sweepClosest - capsuleClosest;
							pushPositionOut(
								sweepNormal,
								combinedRadius - sweepDistance);
						}
					}
				}
				if (resolved)
				{
					node.collisionOutputPosition = position;
					node.hasCollisionOutputPosition = true;

					node.hadContactThisFrame = true;
					node.hadPushThisFrame = true;
					++_desc.debugCollisionHitCount;
				}

				if (useHardCollisionProjection && iter + 1u == iterations)
				{
					auto resolveParentPosition = [&]() -> Vec3
						{
							if (node.parentNodeIndex >= 0)
							{
								const Node& parentNode = chain.nodes[static_cast<size_t>(node.parentNodeIndex)];
								return parentNode.actor != nullptr
									? From_PxVec3(parentNode.actor->getGlobalPose().p)
									: parentNode.solvedPosition;
							}

							return chain.anchorActor != nullptr
								? From_PxVec3(chain.anchorActor->getGlobalPose().p)
								: Get_Position(chain.anchorWorld);
						};

					Vec3 hardPosition = node.hasCollisionOutputPosition
						? node.collisionOutputPosition
						: node.solvedPosition;
					Vec3 hardParentPosition = resolveParentPosition();
					bool hardResolved = false;
					const float hardStepLimit = std::max(
						nodeRadius + skinWidth + 0.0005f,
						node.length * 0.45f);

					auto accumulateHardContact = [&](const Vec3& normal)
						{
							if (normal.LengthSquared() <= 0.0001f)
								return;

							hardResolved = true;
						};

					auto applyHardCorrection = [&](const Vec3& rawNormal, float distance)
						{
							if (distance <= 0.f)
								return false;

							Vec3 normal = rawNormal;
							if (normal.LengthSquared() <= 0.0001f)
								return false;
							normal.Normalize();

							const Vec3 correction = Clamp_VectorLength(
								normal * distance,
								hardStepLimit);
							if (correction.LengthSquared() <= 0.000001f)
								return false;

							hardPosition += correction;
							accumulateHardContact(normal);
							return true;
						};

					auto preserveNodeLength = [&]()
						{
							if (hardLengthAlpha <= 0.f || node.length <= 0.0001f)
								return;

							Vec3 segment = hardPosition - hardParentPosition;
							if (segment.LengthSquared() <= 0.0001f)
								segment = Get_Position(node.originalWorld) - hardParentPosition;
							if (segment.LengthSquared() <= 0.0001f)
								return;

							segment.Normalize();
							const Vec3 lengthLockedPosition = hardParentPosition + segment * node.length;
							hardPosition = Vec3::Lerp(hardPosition, lengthLockedPosition, hardLengthAlpha);
						};

					auto buildFallbackNormal = [&](const Vec3& capsuleClosest) -> Vec3
						{
							Vec3 normal = hardPosition - capsuleClosest;
							if (normal.LengthSquared() <= 0.0001f)
								normal = hardPosition - hardParentPosition;
							if (normal.LengthSquared() <= 0.0001f)
								normal = Get_Position(node.originalWorld) - hardParentPosition;
							if (normal.LengthSquared() <= 0.0001f)
								normal = Vec3::UnitY;
							normal.Normalize();
							return normal;
						};

					auto projectPointAgainstCapsule = [&](const WorldCapsule& capsule)
						{
							const float combinedRadius = nodeRadius + skinWidth + capsule.radius;
							const Vec3 closest = Closest_PointOnSegment(hardPosition, capsule.startPoint, capsule.endPoint);
							Vec3 normal = hardPosition - closest;
							const float distance = normal.Length();
							if (distance >= combinedRadius)
								return false;

							if (distance > 0.0001f)
								normal /= distance;
							else
								normal = buildFallbackNormal(closest);

							return applyHardCorrection(
								normal,
								combinedRadius - distance + 0.0005f);
						};

					auto projectSegmentAgainstCapsule = [&](const WorldCapsule& capsule)
						{
							const float combinedRadius = nodeRadius + skinWidth + capsule.radius;
							Vec3 hairClosest = Vec3::Zero;
							Vec3 capsuleClosest = Vec3::Zero;
							float hairT = 0.f;
							float capsuleT = 0.f;
							const float distanceSq = SegmentSegmentClosestPoints(
								hardParentPosition,
								hardPosition,
								capsule.startPoint,
								capsule.endPoint,
								hairClosest,
								capsuleClosest,
								hairT,
								capsuleT);
							if (distanceSq >= combinedRadius * combinedRadius)
								return false;
							if (hairT <= kHardSegmentMinChildT)
								return false;

							Vec3 normal = hairClosest - capsuleClosest;
							const float distance = sqrtf(std::max(0.f, distanceSq));
							if (distance > 0.0001f)
								normal /= distance;
							else
								normal = buildFallbackNormal(capsuleClosest);

							const float penetration = combinedRadius - distance;
							const float childWeight = std::clamp(hairT, kHardSegmentMinChildT, 1.f);
							return applyHardCorrection(
								normal,
								penetration / childWeight + 0.0005f);
						};

					for (uint32 hardIter = 0; hardIter < hardProjectionIterations; ++hardIter)
					{
						const Vec3 beforeProjection = hardPosition;
						bool projectedThisIteration = false;

						for (const WorldCapsule& capsule : worldCapsules)
						{
							projectedThisIteration = projectPointAgainstCapsule(capsule) || projectedThisIteration;
							projectedThisIteration = projectSegmentAgainstCapsule(capsule) || projectedThisIteration;
						}

						if (projectedThisIteration)
						{
							preserveNodeLength();
							hardParentPosition = resolveParentPosition();
						}

						if (!projectedThisIteration || (hardPosition - beforeProjection).LengthSquared() <= 0.000001f)
							break;
					}

					for (const WorldCapsule& capsule : worldCapsules)
					{
						const float combinedRadius = nodeRadius + skinWidth + capsule.radius;
						Vec3 sweepClosest = Vec3::Zero;
						Vec3 capsuleClosest = Vec3::Zero;
						float sweepT = 0.f;
						float capsuleT = 0.f;
						const float distanceSq = SegmentSegmentClosestPoints(
							node.prevSolvedPosition,
							hardPosition,
							capsule.startPoint,
							capsule.endPoint,
							sweepClosest,
							capsuleClosest,
							sweepT,
							capsuleT);
						if (distanceSq >= combinedRadius * combinedRadius)
							continue;
						if (sweepT <= 0.25f)
							continue;

						const float sweepTravel = Vec3::Distance(node.prevSolvedPosition, hardPosition);
						const float maxSweepTravel = std::max(combinedRadius * 2.5f, node.length * 2.5f);
						if (maxSweepTravel > 0.0001f && sweepTravel > maxSweepTravel)
							continue;

						Vec3 normal = sweepClosest - capsuleClosest;
						const float distance = sqrtf(std::max(0.f, distanceSq));
						if (distance > 0.0001f)
							normal /= distance;
						else
							normal = buildFallbackNormal(capsuleClosest);

						const Vec3 capsuleSurface = capsuleClosest + normal * (combinedRadius + 0.0005f);
						const Vec3 sweepCorrection = capsuleSurface - hardPosition;
						const float sweepAlpha = std::clamp(0.35f + sweepT * 0.25f, 0.f, 0.55f);
						hardPosition += Clamp_VectorLength(sweepCorrection * sweepAlpha, hardStepLimit);
						accumulateHardContact(normal);
						preserveNodeLength();
					}

					if (hardResolved)
					{
						for (uint32 hardIter = 0; hardIter < hardProjectionIterations; ++hardIter)
						{
							const Vec3 beforeProjection = hardPosition;
							bool projectedThisIteration = false;

							for (const WorldCapsule& capsule : worldCapsules)
							{
								projectedThisIteration = projectPointAgainstCapsule(capsule) || projectedThisIteration;
								projectedThisIteration = projectSegmentAgainstCapsule(capsule) || projectedThisIteration;
							}

							if (projectedThisIteration)
							{
								hardParentPosition = resolveParentPosition();
							}

							if (!projectedThisIteration || (hardPosition - beforeProjection).LengthSquared() <= 0.000001f)
								break;
						}
					}

					if (hardResolved)
					{
						const bool wasAlreadyCounted = node.hadPushThisFrame;
						node.collisionOutputPosition = hardPosition;
						node.hasCollisionOutputPosition = true;
						node.hadContactThisFrame = true;
						node.hadPushThisFrame = true;
						if (!wasAlreadyCounted)
							++_desc.debugCollisionHitCount;
					}
				}
			}
		}
	}
}

void PhysicsModifier::Apply_PoseRestoreAndDamping(float timeDelta, bool isOwnerIdle)
{
	if (timeDelta <= 0.f)
		return;

	const float baseDampingAlpha = std::clamp(_desc.releaseInertiaDamping, 0.f, 0.98f);
	const float dampingAlpha = isOwnerIdle
		? std::max(baseDampingAlpha, std::clamp(_desc.idleOutputSmoothAlpha, 0.f, 0.98f))
		: baseDampingAlpha;
	const float restoreAlpha = std::clamp(
		_desc.poseRestoreAlpha,
		0.f,
		1.f);
	const float idlePositionRestoreAlpha = std::clamp(
		_desc.poseRestoreAlpha,
		0.f,
		1.f);
	const bool windActive =
		_desc.windScale > 0.001f &&
		_desc.windDirection.LengthSquared() > 0.001f;
	const float idleSettleDistance = std::max(
		_desc.idleMotionDeadzone * 0.20f,
		_desc.poseRestoreMaxSpeed * timeDelta * 0.05f);
	const float idleSettleDistanceSq = idleSettleDistance * idleSettleDistance;
	const float idleSettleSpeed = std::max(
		_desc.poseRestoreMaxSpeed * 0.01f,
		0.0001f);
	const float idleSettleSpeedSq = idleSettleSpeed * idleSettleSpeed;

	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			if (node.isKinematic || node.actor == nullptr)
				continue;

			const PxTransform actorPose = node.actor->getGlobalPose();
			const Vec3 currentPosition = From_PxVec3(actorPose.p);
			Matrix restParentWorld = chain.anchorWorld;
			Vec3 parentPosition = chain.anchorActor != nullptr
				? From_PxVec3(chain.anchorActor->getGlobalPose().p)
				: Get_Position(chain.anchorWorld);
			if (node.parentNodeIndex >= 0)
			{
				const Node& parentNode = chain.nodes[static_cast<size_t>(node.parentNodeIndex)];
				restParentWorld = parentNode.originalWorld;
				parentPosition = parentNode.actor != nullptr
					? From_PxVec3(parentNode.actor->getGlobalPose().p)
					: parentNode.solvedPosition;
			}
			Vec3 restWorldDir = Vec3::Zero;
			const Vec3 restLocalDir = node.poseLocalDir.LengthSquared() > 0.0001f
				? node.poseLocalDir
				: node.bindLocalDir;
			if (restLocalDir.LengthSquared() > 0.0001f)
			{
				restWorldDir = Vec3::TransformNormal(restLocalDir, restParentWorld);
				if (restWorldDir.LengthSquared() > 0.0001f)
					restWorldDir.Normalize();
			}
			const Vec3 restPosition = restWorldDir.LengthSquared() > 0.0001f
				? parentPosition + restWorldDir * node.length
				: Get_Position(node.originalWorld);
			Vec3 correctedPosition = currentPosition;

			Vec3 linearVelocity = From_PxVec3(node.actor->getLinearVelocity());
			if (restoreAlpha > 0.f && _desc.poseRestoreMaxSpeed > 0.f)
			{
				Vec3 restDelta = restPosition - currentPosition;
				const float restDistance = restDelta.Length();
				if (restDistance > 0.0001f)
				{
					restDelta /= restDistance;
					const float maxRestoreSpeed = _desc.poseRestoreMaxSpeed * std::max(0.10f, chain.desc.restoreMul);
					const Vec3 restoreVelocity = restDelta * std::min(restDistance / timeDelta, maxRestoreSpeed);
					linearVelocity = Vec3::Lerp(linearVelocity, restoreVelocity, restoreAlpha);
				}
			}

			if (isOwnerIdle && idlePositionRestoreAlpha > 0.f)
			{
				correctedPosition = Vec3::Lerp(currentPosition, restPosition, idlePositionRestoreAlpha);
				PxTransform correctedPose = actorPose;
				correctedPose.p = To_PxVec3(correctedPosition);
				node.actor->setGlobalPose(correctedPose);
			}

			if (dampingAlpha > 0.f)
				linearVelocity *= (1.f - dampingAlpha);

			Vec3 angularVelocity = From_PxVec3(node.actor->getAngularVelocity());
			const float angularDampingAlpha = std::clamp(
				dampingAlpha,
				0.f,
				0.98f);
			angularVelocity *= (1.f - angularDampingAlpha);
			bool shouldSleep = false;

			if (isOwnerIdle && !windActive)
			{
				const float restDistanceSq = (correctedPosition - restPosition).LengthSquared();
				if (restDistanceSq <= idleSettleDistanceSq
					&& linearVelocity.LengthSquared() <= idleSettleSpeedSq
					&& angularVelocity.LengthSquared() <= idleSettleSpeedSq)
				{
					correctedPosition = restPosition;
					PxTransform settledPose = actorPose;
					settledPose.p = To_PxVec3(restPosition);
					node.actor->setGlobalPose(settledPose);
					linearVelocity = Vec3::Zero;
					angularVelocity = Vec3::Zero;
					node.smoothedInertiaVelocity = Vec3::Zero;
					node.hasSmoothedInertiaVelocity = false;
					shouldSleep = true;
				}
			}

			node.actor->setLinearVelocity(To_PxVec3(linearVelocity), !shouldSleep);
			node.actor->setAngularVelocity(To_PxVec3(angularVelocity), !shouldSleep);
			if (shouldSleep)
				node.actor->putToSleep();
		}
	}
}

void PhysicsModifier::Apply_LocalOffsetConstraints(float timeDelta, bool isOwnerIdle)
{
	Vec3 maxLocalOffset(
		std::max(0.f, fabsf(_desc.maxLocalOffset.x)),
		std::max(0.f, fabsf(_desc.maxLocalOffset.y)),
		std::max(0.f, fabsf(_desc.maxLocalOffset.z)));
	if (!Has_LocalOffsetLimit(maxLocalOffset))
		return;

	const float idleRestoreAlpha = isOwnerIdle
		? std::clamp(_desc.poseRestoreAlpha, 0.f, 1.f)
		: 0.f;
	const float idleVelocityDamp = isOwnerIdle
		? std::max(
			std::clamp(_desc.releaseInertiaDamping, 0.f, 0.98f),
			std::clamp(_desc.idleOutputSmoothAlpha, 0.f, 0.98f))
		: 0.f;

	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			if (node.isKinematic)
				continue;

			Matrix restParentWorld = chain.anchorWorld;
			Vec3 parentPosition = chain.anchorActor != nullptr
				? From_PxVec3(chain.anchorActor->getGlobalPose().p)
				: Get_Position(chain.anchorWorld);
			if (node.parentNodeIndex >= 0)
			{
				const Node& parentNode = chain.nodes[static_cast<size_t>(node.parentNodeIndex)];
				restParentWorld = parentNode.originalWorld;
				parentPosition = parentNode.actor != nullptr
					? From_PxVec3(parentNode.actor->getGlobalPose().p)
					: parentNode.solvedPosition;
			}

			const Matrix parentWorld = Local_With_Position(restParentWorld, parentPosition);
			const Matrix parentInv = parentWorld.Invert();
			const Vec3 restLocalPosition = Vec3::Transform(Get_Position(node.originalWorld), restParentWorld.Invert());
			const Vec3 restPosition = Vec3::Transform(restLocalPosition, parentWorld);
			const Vec3 currentPosition = node.solvedPosition;
			const Vec3 localOffset = Vec3::TransformNormal(currentPosition - restPosition, parentInv);

			bool wasClamped = false;
			Vec3 constrainedLocalOffset = Clamp_LocalOffset(localOffset, maxLocalOffset, wasClamped);
			Vec3 constrainedPosition = restPosition + Vec3::TransformNormal(constrainedLocalOffset, parentWorld);

			if (idleRestoreAlpha > 0.f)
				constrainedPosition = Vec3::Lerp(constrainedPosition, restPosition, idleRestoreAlpha);

			const bool shouldApply = wasClamped
				|| (idleRestoreAlpha > 0.f && (constrainedPosition - currentPosition).LengthSquared() > 0.000001f);
			if (!shouldApply)
				continue;

			float maxOverflowRatio = 0.f;
			if (wasClamped)
			{
				if (maxLocalOffset.x > 0.0001f)
					maxOverflowRatio = std::max(maxOverflowRatio, fabsf(localOffset.x) / maxLocalOffset.x);
				if (maxLocalOffset.y > 0.0001f)
					maxOverflowRatio = std::max(maxOverflowRatio, fabsf(localOffset.y) / maxLocalOffset.y);
				if (maxLocalOffset.z > 0.0001f)
					maxOverflowRatio = std::max(maxOverflowRatio, fabsf(localOffset.z) / maxLocalOffset.z);
			}

			const bool escapedConstraintFrame = maxOverflowRatio >= 2.5f;
			if (wasClamped && !escapedConstraintFrame && !isOwnerIdle && timeDelta > 0.0001f)
			{
				Vec3 correction = constrainedPosition - currentPosition;
				const float correctionLength = correction.Length();
				if (correctionLength > 0.0001f)
				{
					const float correctionAlpha = std::clamp(
						1.f - std::clamp(_desc.releaseInertiaDamping, 0.f, 0.98f),
						std::clamp(_desc.outputSmoothAlpha, 0.f, 1.f),
						1.f);
					const float correctionMaxStep = std::max(0.f, _desc.poseRestoreMaxSpeed) * timeDelta;
					correction *= correctionAlpha;
					const float smoothedLength = correction.Length();
					if (correctionMaxStep > 0.0001f && smoothedLength > correctionMaxStep)
					{
						correction /= smoothedLength;
						correction *= correctionMaxStep;
					}
					constrainedPosition = currentPosition + correction;
				}
			}

			node.solvedPosition = constrainedPosition;
			if (wasClamped)
			{
				++_desc.debugLocalClampCount;
				if (fabsf(constrainedLocalOffset.x - localOffset.x) > 0.000001f)
					++_desc.debugLocalClampXCount;
				if (fabsf(constrainedLocalOffset.y - localOffset.y) > 0.000001f)
					++_desc.debugLocalClampYCount;
				if (fabsf(constrainedLocalOffset.z - localOffset.z) > 0.000001f)
					++_desc.debugLocalClampZCount;
			}

			if (node.actor == nullptr)
				continue;

			PxTransform pose = node.actor->getGlobalPose();
			pose.p = To_PxVec3(constrainedPosition);
			node.actor->setGlobalPose(pose);

			Vec3 linearVelocity = From_PxVec3(node.actor->getLinearVelocity());
			Vec3 localVelocity = Vec3::TransformNormal(linearVelocity, parentInv);
			if (escapedConstraintFrame)
			{
				localVelocity = Vec3::Zero;
			}
			else if (wasClamped)
			{
				const float clampVelocityKeep = std::clamp(
					1.f - std::clamp(_desc.releaseInertiaDamping, 0.f, 0.98f),
					0.f,
					0.35f);
				if (maxLocalOffset.x > 0.0001f
					&& fabsf(localOffset.x) >= maxLocalOffset.x - 0.0001f
					&& localOffset.x * localVelocity.x > 0.f)
					localVelocity.x *= clampVelocityKeep;
				if (maxLocalOffset.y > 0.0001f
					&& fabsf(localOffset.y) >= maxLocalOffset.y - 0.0001f
					&& localOffset.y * localVelocity.y > 0.f)
					localVelocity.y *= clampVelocityKeep;
				if (maxLocalOffset.z > 0.0001f
					&& fabsf(localOffset.z) >= maxLocalOffset.z - 0.0001f
					&& localOffset.z * localVelocity.z > 0.f)
					localVelocity.z *= clampVelocityKeep;
			}
			if (idleVelocityDamp > 0.f)
				localVelocity *= (1.f - idleVelocityDamp);

			linearVelocity = Vec3::TransformNormal(localVelocity, parentWorld);
			node.actor->setLinearVelocity(To_PxVec3(linearVelocity));
			if (isOwnerIdle)
				node.actor->setAngularVelocity(node.actor->getAngularVelocity() * (1.f - idleVelocityDamp));
		}
	}
}

void PhysicsModifier::Write_BackToModel(ModelCom* model, const Matrix& ownerWorld, bool isOwnerIdle)
{
	if (model == nullptr) return;

	const float smoothAlpha = std::clamp(isOwnerIdle ? _desc.idleOutputSmoothAlpha : _desc.outputSmoothAlpha, 0.f, 1.f);
	const float overallOutputAlpha = std::clamp(_desc.outputAlpha, 0.f, 1.f);
	bool wroteAnyBone = false;

	const auto getNodePosition = [](const Node& node) -> Vec3
		{
			if (node.actor != nullptr)
				return From_PxVec3(node.actor->getGlobalPose().p);
			return node.solvedPosition;
		};

	auto writeNodeRotation =
		[this, model, smoothAlpha, overallOutputAlpha, isOwnerIdle](
			Chain& chain,
			Node& node,
			const Vec3& restLocalDirection,
			const Vec3& simulatedLocalDirection,
			const Vec3& simulatedLocalTranslation,
			bool allowTranslationOutput,
			float outputDepthRatio) -> bool
		{
			Vec3 restLocalDir = restLocalDirection;
			Vec3 targetLocalDir = simulatedLocalDirection;
			if (restLocalDir.LengthSquared() <= 0.0001f || targetLocalDir.LengthSquared() <= 0.0001f)
				return false;

			restLocalDir.Normalize();
			targetLocalDir.Normalize();

			const float clampedDepth = std::clamp(outputDepthRatio, 0.f, 1.f);
			const float nodeAngleLimit = Compute_NodeAngleLimitDegree(chain, clampedDepth);
			targetLocalDir = Limit_LocalDirectionByAngle(restLocalDir, targetLocalDir, nodeAngleLimit);

			const float outputRestoreAlpha = std::clamp(
				isOwnerIdle ? _desc.poseRestoreAlpha : 0.f,
				0.f,
				1.f);
			if (outputRestoreAlpha > 0.f)
			{
				targetLocalDir = Vec3::Lerp(targetLocalDir, restLocalDir, outputRestoreAlpha);
				if (targetLocalDir.LengthSquared() > 0.0001f)
					targetLocalDir.Normalize();
				else
					targetLocalDir = restLocalDir;
			}

			const float minimumFollowAlpha = _desc.freeRotationFollowAlpha - clampedDepth * _desc.rotationFollowDepthScale;
			const float baseRotationFollowAlpha = std::max(
				std::clamp(minimumFollowAlpha, 0.f, 1.f),
				std::clamp(smoothAlpha * chain.desc.smoothMultiplier, 0.f, 1.f));
			const float depthSmoothWeight = SmoothStep(clampedDepth);
			const float rotationFollowAlpha = baseRotationFollowAlpha;
			if (node.hasSmoothedLocalDirection &&
				node.smoothedLocalDirection.Dot(targetLocalDir) > -std::clamp(smoothAlpha, 0.f, 1.f))
			{
				targetLocalDir = Vec3::Lerp(node.smoothedLocalDirection, targetLocalDir, rotationFollowAlpha);
				if (targetLocalDir.LengthSquared() > 0.0001f)
					targetLocalDir.Normalize();
				else
					targetLocalDir = restLocalDir;
			}
			node.smoothedLocalDirection = targetLocalDir;
			node.hasSmoothedLocalDirection = true;

			Quat poseRotation = Extract_Rotation(node.poseLocal);
			poseRotation.Normalize();

			Quat solvedRotation = Build_RotationDelta(restLocalDir, targetLocalDir) * poseRotation;
			solvedRotation.Normalize();

			const float depthOutputAlpha = std::clamp(
				std::lerp(std::clamp(chain.desc.smoothMultiplier, 0.f, 1.f), 1.f, depthSmoothWeight),
				0.f,
				1.f);
			float rotationDepthGate = 1.f;
			const float rotationDepthStart = std::clamp(_desc.rotationOutputDepthStart, 0.f, 1.f);
			const float rotationDepthEnd = std::clamp(_desc.rotationOutputDepthEnd, 0.f, 1.f);
			if (rotationDepthEnd > rotationDepthStart + 0.0001f)
			{
				const float depthT = std::clamp((clampedDepth - rotationDepthStart) / (rotationDepthEnd - rotationDepthStart), 0.f, 1.f);
				rotationDepthGate = std::clamp(_desc.rootRotationOutputScale, 0.f, 1.f)
					+ (1.f - std::clamp(_desc.rootRotationOutputScale, 0.f, 1.f)) * SmoothStep(depthT);
			}
			const float nodeRotationOutputAlpha = std::clamp(
				overallOutputAlpha * chain.desc.outputMultiplier * _desc.rotationOutputAlpha * depthOutputAlpha * rotationDepthGate,
				0.f,
				1.f);
			Quat blendedRotation = Quat::Slerp(poseRotation, solvedRotation, nodeRotationOutputAlpha);
			blendedRotation.Normalize();

			const bool hadSmoothedLocalTransform = node.hasSmoothedLocalTransform;
			if (hadSmoothedLocalTransform)
			{
				node.smoothedLocalRotation = Quat::Slerp(node.smoothedLocalRotation, blendedRotation, rotationFollowAlpha);
			}
			else
			{
				node.smoothedLocalRotation = blendedRotation;
				node.hasSmoothedLocalTransform = true;
			}
			node.smoothedLocalRotation.Normalize();

			Vec3 poseScale = Vec3::One;
			Quat unusedPoseRotation = Quat::Identity;
			Vec3 translation = Vec3::Zero;
			Matrix poseLocal = node.poseLocal;
			poseLocal.Decompose(poseScale, unusedPoseRotation, translation);
			if (poseScale.LengthSquared() <= 0.000001f)
				poseScale = node.bindScale;

			auto matchRestTranslationLength = [&translation](Vec3 targetTranslation) -> Vec3
				{
					const float restTranslationLength = translation.Length();
					const float targetTranslationLength = targetTranslation.Length();
					if (restTranslationLength > 0.0001f && targetTranslationLength > 0.0001f)
					{
						targetTranslation /= targetTranslationLength;
						targetTranslation *= restTranslationLength;
					}
					return targetTranslation;
				};

			Vec3 targetTranslation = translation;
			if (allowTranslationOutput)
			{
				targetTranslation = matchRestTranslationLength(simulatedLocalTranslation);

				Vec3 translationOffset = targetTranslation - translation;
				Vec3 maxLocalOffset(
					std::max(0.f, fabsf(_desc.maxLocalOffset.x)),
					std::max(0.f, fabsf(_desc.maxLocalOffset.y)),
					std::max(0.f, fabsf(_desc.maxLocalOffset.z)));
				if (Has_LocalOffsetLimit(maxLocalOffset))
				{
					bool unusedClamped = false;
					translationOffset = Clamp_LocalOffset(translationOffset, maxLocalOffset, unusedClamped);
					targetTranslation = translation + translationOffset;
				}
			}

			if (hadSmoothedLocalTransform)
			{
				const float translationFollowAlpha = allowTranslationOutput
					? std::clamp(
						std::lerp(
							smoothAlpha,
							_desc.freeRotationFollowAlpha,
							std::clamp(chain.desc.smoothMultiplier, 0.f, 1.f)),
						0.f,
						1.f)
					: std::clamp(
						std::max(
							rotationFollowAlpha,
							std::lerp(smoothAlpha, _desc.freeRotationFollowAlpha, depthSmoothWeight)),
						0.f,
						1.f);
				node.smoothedLocalTranslation = Vec3::Lerp(node.smoothedLocalTranslation, targetTranslation, translationFollowAlpha);
			}
			else
			{
				node.smoothedLocalTranslation = targetTranslation;
			}

			Matrix finalLocal = Build_LocalMatrix_FromSRT(poseScale, node.smoothedLocalRotation, node.smoothedLocalTranslation);
			model->Set_BoneLocalMatrix(node.boneIndex, finalLocal);
			return true;
		};

	auto applyCollisionOutputTranslation =
		[this, model, &ownerWorld](Chain& chain, Node& node) -> bool
		{
			if (!node.hasCollisionOutputPosition || node.parentNodeIndex < 0)
				return false;

			const Node& parentNode = chain.nodes[static_cast<size_t>(node.parentNodeIndex)];
			const Matrix* parentCombined = model->Get_BoneMatrixPtr(parentNode.boneIndex);
			const Matrix parentWorld = parentCombined != nullptr
				? (*parentCombined) * ownerWorld
				: parentNode.originalWorld;

			Matrix currentLocal = model->Get_BoneLocalMatrix(node.boneIndex);
			Vec3 localScale = Vec3::One;
			Quat localRotation = Quat::Identity;
			Vec3 unusedTranslation = Vec3::Zero;
			currentLocal.Decompose(localScale, localRotation, unusedTranslation);
			if (localScale.LengthSquared() <= 0.000001f)
				localScale = node.bindScale;
			localRotation.Normalize();

			const Vec3 collisionLocalTranslation = Vec3::Transform(node.collisionOutputPosition, parentWorld.Invert());
			const Matrix finalLocal = Build_LocalMatrix_FromSRT(localScale, localRotation, collisionLocalTranslation);
			model->Set_BoneLocalMatrix(node.boneIndex, finalLocal);
			return true;
		};

	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			if (node.parentNodeIndex >= 0 || node.childNodeIndices.empty())
				continue;

			const int32 childNodeIndex = node.childNodeIndices.front();
			if (childNodeIndex < 0 || static_cast<size_t>(childNodeIndex) >= chain.nodes.size())
				continue;

			Node& childNode = chain.nodes[static_cast<size_t>(childNodeIndex)];
			Vec3 simulatedWorldDir = getNodePosition(childNode) - getNodePosition(node);
			if (simulatedWorldDir.LengthSquared() <= 0.0001f)
				continue;

			simulatedWorldDir.Normalize();
			const Vec3 simulatedLocalDir = Get_LocalDirection(node.originalWorld, simulatedWorldDir);
			const Vec3 restLocalDir = childNode.poseLocalDir.LengthSquared() > 0.0001f
				? childNode.poseLocalDir
				: childNode.bindLocalDir;

			wroteAnyBone = writeNodeRotation(
				chain,
				node,
				restLocalDir,
				simulatedLocalDir,
				Get_Position(node.poseLocal),
				false,
				childNode.depthRatio) || wroteAnyBone;
		}
	}

	if (wroteAnyBone)
		model->Rebuild_BoneCombinedMatrices();

	for (auto& chain : _chains)
	{
		for (size_t i = 0; i < chain.nodes.size(); ++i)
		{
			auto& node = chain.nodes[i];
			if (node.isKinematic || node.parentNodeIndex < 0)
				continue;

			if (!node.childNodeIndices.empty())
			{
				const int32 childNodeIndex = node.childNodeIndices.front();
				if (childNodeIndex >= 0 && static_cast<size_t>(childNodeIndex) < chain.nodes.size())
				{
					Node& childNode = chain.nodes[static_cast<size_t>(childNodeIndex)];
					Vec3 simulatedWorldDir = getNodePosition(childNode) - getNodePosition(node);
					if (simulatedWorldDir.LengthSquared() > 0.0001f)
					{
						simulatedWorldDir.Normalize();
						const Vec3 simulatedLocalDir = Get_LocalDirection(node.originalWorld, simulatedWorldDir);
						const Vec3 restLocalDir = childNode.poseLocalDir.LengthSquared() > 0.0001f
							? childNode.poseLocalDir
							: childNode.bindLocalDir;

						if (writeNodeRotation(
							chain,
							node,
							restLocalDir,
							simulatedLocalDir,
							Get_Position(node.poseLocal),
							false,
							childNode.depthRatio))
						{
							wroteAnyBone = true;
							model->Rebuild_BoneCombinedMatrices();
						}
					}
				}

				continue;
			}

			Node& parentNode = chain.nodes[static_cast<size_t>(node.parentNodeIndex)];
			const Matrix* parentCombined = model->Get_BoneMatrixPtr(parentNode.boneIndex);
			const Matrix parentWorld = parentCombined != nullptr
				? (*parentCombined) * ownerWorld
				: parentNode.originalWorld;

			const Vec3 nodePhysicsPosition = getNodePosition(node);
			const bool forceTranslationOutput = node.hasCollisionOutputPosition;
			const Vec3 nodeOutputPosition = forceTranslationOutput
				? node.collisionOutputPosition
				: nodePhysicsPosition;
			Vec3 simulatedWorldDir = nodePhysicsPosition - getNodePosition(parentNode);
			if (simulatedWorldDir.LengthSquared() <= 0.0001f && forceTranslationOutput)
				simulatedWorldDir = nodeOutputPosition - getNodePosition(parentNode);
			if (simulatedWorldDir.LengthSquared() <= 0.0001f)
				continue;

			simulatedWorldDir.Normalize();
			const Vec3 simulatedLocalDir = Get_LocalDirection(parentWorld, simulatedWorldDir);
			const Vec3 simulatedLocalTranslation =
				Vec3::Transform(nodePhysicsPosition, parentWorld.Invert());
			const Vec3 restLocalDir = node.poseLocalDir.LengthSquared() > 0.0001f
				? node.poseLocalDir
				: node.bindLocalDir;

			if (writeNodeRotation(
				chain,
				node,
				restLocalDir,
				simulatedLocalDir,
				simulatedLocalTranslation,
				_desc.enableTranslationOutput,
				node.depthRatio))
			{
				wroteAnyBone = true;
				model->Rebuild_BoneCombinedMatrices();
			}
		}
	}

	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			if (node.isKinematic)
				continue;

			if (applyCollisionOutputTranslation(chain, node))
			{
				wroteAnyBone = true;
				model->Rebuild_BoneCombinedMatrices();
			}
		}
	}

	if (wroteAnyBone)
		model->Rebuild_BoneCombinedMatrices();
}

void PhysicsModifier::Trace_BoneMotion(ModelCom* model, const Matrix& ownerWorld)
{
	if (!_desc.debugTraceBoneMotion || model == nullptr)
		return;

	const bool traceMaelleBackHair = _desc.modifierTag.find(L"MaelleBackHair") != wstring::npos;
	const bool traceSimonHair = _desc.modifierTag.find(L"SimonHair") != wstring::npos;
	if (!traceMaelleBackHair && !traceSimonHair)
		return;

	const int32 maxTraceFrames = 900;
	if (_desc.debugTraceFrameCount >= maxTraceFrames)
		return;

	const int32 frame = _desc.debugTraceFrameCount++;
	std::ofstream out(traceSimonHair ? "SimonHairPhysicsTrace.csv" : "MaelleHairPhysicsTrace.csv", std::ios::app);
	if (!out.is_open())
		return;

	if (frame == 0)
	{
		out << "frame,chainRoot,bone,parent,depth,poseDelta,solvedDelta,actorVelocity,outputAngleDeg,limitedAngleDeg,clampCount,projectCount,inertia,anchorAdvect\n";
	}

	_desc.debugTraceMaxNodeMotion = 0.f;
	_desc.debugTraceMaxOutputAngle = 0.f;

	const auto getNodePosition = [](const Node& node) -> Vec3
		{
			if (node.actor != nullptr)
				return From_PxVec3(node.actor->getGlobalPose().p);
			return node.solvedPosition;
		};

	const auto getNodeVelocity = [](const Node& node) -> float
		{
			if (node.actor == nullptr)
				return 0.f;
			return From_PxVec3(node.actor->getLinearVelocity()).Length();
		};

	for (const Chain& chain : _chains)
	{
		for (const Node& node : chain.nodes)
		{
			if (node.isKinematic && node.childNodeIndices.empty())
				continue;

			const Node* directionNode = &node;
			const Node* parentNode = nullptr;
			Matrix parentWorld = Matrix::Identity;
			float outputDepthRatio = node.depthRatio;

			if (node.parentNodeIndex >= 0)
			{
				parentNode = &chain.nodes[static_cast<size_t>(node.parentNodeIndex)];
				const Matrix* parentCombined = model->Get_BoneMatrixPtr(parentNode->boneIndex);
				parentWorld = parentCombined != nullptr
					? (*parentCombined) * ownerWorld
					: parentNode->originalWorld;
			}
			else if (!node.childNodeIndices.empty())
			{
				const int32 childNodeIndex = node.childNodeIndices.front();
				if (childNodeIndex < 0 || static_cast<size_t>(childNodeIndex) >= chain.nodes.size())
					continue;

				directionNode = &chain.nodes[static_cast<size_t>(childNodeIndex)];
				parentNode = &node;
				parentWorld = node.originalWorld;
				outputDepthRatio = directionNode->depthRatio;
			}
			else
			{
				continue;
			}

			Vec3 simulatedWorldDir = getNodePosition(*directionNode) - getNodePosition(*parentNode);
			if (simulatedWorldDir.LengthSquared() <= 0.0001f)
				continue;
			simulatedWorldDir.Normalize();

			Vec3 simulatedLocalDir = Get_LocalDirection(parentWorld, simulatedWorldDir);
			if (simulatedLocalDir.LengthSquared() <= 0.0001f)
				continue;
			simulatedLocalDir.Normalize();

			Vec3 restLocalDir = directionNode->poseLocalDir.LengthSquared() > 0.0001f
				? directionNode->poseLocalDir
				: directionNode->bindLocalDir;
			if (restLocalDir.LengthSquared() <= 0.0001f)
				continue;
			restLocalDir.Normalize();

			const float dot = std::clamp(restLocalDir.Dot(simulatedLocalDir), -1.f, 1.f);
			const float outputAngleDeg = XMConvertToDegrees(acosf(dot));
			const float limitedAngleDeg = Compute_NodeAngleLimitDegree(chain, std::clamp(outputDepthRatio, 0.f, 1.f));
			const float solvedDelta = Vec3::Distance(node.solvedPosition, node.prevSolvedPosition);
			const float poseDelta = node.poseMotionDelta.Length();

			_desc.debugTraceMaxNodeMotion = std::max(_desc.debugTraceMaxNodeMotion, solvedDelta);
			_desc.debugTraceMaxOutputAngle = std::max(_desc.debugTraceMaxOutputAngle, outputAngleDeg);

			const string& boneName = model->Get_BoneName(node.boneIndex);
			const string parentName = parentNode != nullptr
				? model->Get_BoneName(parentNode->boneIndex)
				: string{};

			out
				<< frame << ','
				<< EscapeCsv(chain.desc.rootBoneName) << ','
				<< EscapeCsv(boneName) << ','
				<< EscapeCsv(parentName) << ','
				<< node.depthRatio << ','
				<< poseDelta << ','
				<< solvedDelta << ','
				<< getNodeVelocity(node) << ','
				<< outputAngleDeg << ','
				<< limitedAngleDeg << ','
				<< _desc.debugLocalClampCount << ','
				<< _desc.debugConstraintProjectionCount << ','
				<< _desc.debugInertiaMagnitude << ','
				<< (_desc.advectSimulationWithAnchor ? 1 : 0)
				<< '\n';
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

		const int32 boneIndex = collisionModel->Get_BoneIndex(collider.boneName);
		if (boneIndex < 0) continue;

		const Matrix* boneCombined = collisionModel->Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
		if (boneCombined == nullptr) continue;

		Matrix boneWorld = (*boneCombined) * collisionOwnerWorld;
		Vec3 worldScale = Vec3::One;
		Quat worldRotation = Quat::Identity;
		Vec3 worldTranslation = Vec3::Zero;
		Matrix boneWorldForDecompose = boneWorld;
		boneWorldForDecompose.Decompose(worldScale, worldRotation, worldTranslation);

		Vec3 localAxis = Vec3::TransformNormal(Vec3::UnitY, Matrix::CreateFromQuaternion(collider.rotationQuat));
		if (localAxis.LengthSquared() <= 0.0001f)
			localAxis = Vec3::UnitY;
		else
			localAxis.Normalize();

		Vec3 axis = Vec3::TransformNormal(localAxis, Matrix::CreateFromQuaternion(worldRotation));
		if (axis.LengthSquared() <= 0.0001f)
			axis = Vec3::UnitY;
		else
			axis.Normalize();

		Vec3 scaledLocalAxis(
			localAxis.x * fabsf(worldScale.x),
			localAxis.y * fabsf(worldScale.y),
			localAxis.z * fabsf(worldScale.z));
		float lengthScale = scaledLocalAxis.Length();
		if (lengthScale <= 0.0001f)
			lengthScale = 1.f;

		const float primitiveScale = std::max(0.001f, _desc.collisionPrimitiveScale);
		const float radiusScale = std::max(std::max(fabsf(worldScale.x), fabsf(worldScale.y)), fabsf(worldScale.z));
		const float scaledHalfHeight = collider.halfLength * primitiveScale * std::max(0.001f, lengthScale);
		const Vec3 center = Vec3::Transform(collider.center, boneWorld);
		
		WorldCapsule cap;
		cap.radius = collider.radius * primitiveScale * std::max(0.001f, radiusScale) + _desc.collisionRadiusPadding;
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

	_desc.debugActiveChainCount = static_cast<int32>(_chains.size());
	_desc.debugActiveNodeCount = 0;
	for (const Chain& chain : _chains)
	{
		for (const Node& node : chain.nodes)
		{
			if (!node.isKinematic)
				++_desc.debugActiveNodeCount;
		}
	}

	const ModelCom* collisionModel = _desc.collisionModel ? _desc.collisionModel : model;
	const Matrix collisionOwnerWorld = _desc.collisionOwnerWorldMatrix ? *_desc.collisionOwnerWorldMatrix : ownerWorld;
	const vector<WorldCapsule> worldCapsules = Build_WorldCapsules(collisionModel, collisionOwnerWorld);
	_desc.debugLoadedCapsuleCount = static_cast<int32>(_colliders.size());
	_desc.debugActiveCapsuleCount = static_cast<int32>(worldCapsules.size());

	if (!_desc.debugDrawCollisionCapsules)
		return;

	const float nodeRadius = std::max(0.02f, _desc.collisionSphereRadius);
	const float skinWidth = std::max(0.f, _desc.collisionContactSkinWidth);
	for (const WorldCapsule& capsule : worldCapsules)
	{
		RuntimeDebugCapsule debugCapsule{};
		debugCapsule.startPoint = capsule.startPoint;
		debugCapsule.endPoint = capsule.endPoint;
		debugCapsule.sourceRadius = std::max(0.f, capsule.radius - _desc.collisionRadiusPadding);
		debugCapsule.collisionRadius = capsule.radius + nodeRadius + skinWidth;
		_runtimeDebugCapsules.push_back(debugCapsule);
	}

	for (const Chain& chain : _chains)
	{
		Vec3 anchorPosition = Get_Position(chain.anchorWorld);
		if (chain.anchorActor != nullptr)
			anchorPosition = From_PxVec3(chain.anchorActor->getGlobalPose().p);

		for (const Node& node : chain.nodes)
		{
			Vec3 actorPosition = node.solvedPosition;
			if (node.actor != nullptr)
				actorPosition = From_PxVec3(node.actor->getGlobalPose().p);
			const Vec3 displayPosition = node.hasCollisionOutputPosition
				? node.collisionOutputPosition
				: actorPosition;

			if (node.debugSphereRadius > 0.f)
			{
				RuntimeDebugSphere sphere{};
				sphere.center = displayPosition;
				sphere.radius = node.debugSphereRadius;
				sphere.hasContact = node.hadContactThisFrame;
				_runtimeDebugSpheres.push_back(sphere);
			}

			RuntimeDebugSegment segment{};
			if (node.parentNodeIndex >= 0)
			{
				const Node& parentNode = chain.nodes[static_cast<size_t>(node.parentNodeIndex)];
				if (parentNode.hasCollisionOutputPosition)
				{
					segment.startPoint = parentNode.collisionOutputPosition;
				}
				else
				{
					segment.startPoint = parentNode.actor != nullptr
						? From_PxVec3(parentNode.actor->getGlobalPose().p)
						: parentNode.solvedPosition;
				}
			}
			else
			{
				segment.startPoint = anchorPosition;
			}

			segment.endPoint = displayPosition;
			segment.hasContact = node.hadContactThisFrame;
			segment.hasPush = node.hadPushThisFrame;
			_runtimeDebugSegments.push_back(segment);
		}
	}
}

bool PhysicsModifier::Validate_RootAlignment(ModelCom* model, const Matrix& ownerWorld)
{
	if (model == nullptr || _chains.empty())
		return true;

	constexpr float kSevereRootMismatch = 150.f;
	for (const Chain& chain : _chains)
	{
		if (chain.nodes.empty())
			continue;

		const Node& rootNode = chain.nodes.front();
		const Matrix* rootCombined = model->Get_BoneMatrixPtr(rootNode.boneIndex);
		if (rootCombined == nullptr || rootNode.actor == nullptr)
			continue;

		const Vec3 rootPosition = Get_Position((*rootCombined) * ownerWorld);
		const Vec3 actorPosition = From_PxVec3(rootNode.actor->getGlobalPose().p);
		if (Vec3::Distance(rootPosition, actorPosition) > kSevereRootMismatch)
			return false;
	}

	return true;
}

const ClassReflectionInfo* PhysicsModifier::Get_RuntimeReflectionInfo() const
{
	static ClassReflectionInfo info = []()
		{
			ClassReflectionInfo result{};
			result.className = "PhysicsModifierRuntime";
			result.displayName = "Physics Modifier Runtime";
			result.category = "Animation";
			result.type = RegisteredType::None;

			AddRuntimeProperty(result.properties, PropertyType::Bool, "writeBackEnabled", "Write Back", PhysicsOffset(&PhysicsModifierDesc::writeBackEnabled));
			AddRuntimeProperty(result.properties, PropertyType::Bool, "enableCollision", "Collision", PhysicsOffset(&PhysicsModifierDesc::enableCollision));
			AddRuntimeProperty(result.properties, PropertyType::Bool, "debugDrawCollisionCapsules", "Debug Col", PhysicsOffset(&PhysicsModifierDesc::debugDrawCollisionCapsules));
			AddRuntimeProperty(result.properties, PropertyType::Bool, "debugTraceBoneMotion", "Trace Bone", PhysicsOffset(&PhysicsModifierDesc::debugTraceBoneMotion));
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugActiveChainCount", "Dbg Chain", PhysicsOffset(&PhysicsModifierDesc::debugActiveChainCount), 1.f, 0.f, 256.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugActiveNodeCount", "Dbg Node", PhysicsOffset(&PhysicsModifierDesc::debugActiveNodeCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugLoadedCapsuleCount", "Dbg Col Load", PhysicsOffset(&PhysicsModifierDesc::debugLoadedCapsuleCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugActiveCapsuleCount", "Dbg Col Act", PhysicsOffset(&PhysicsModifierDesc::debugActiveCapsuleCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugCollisionHitCount", "Dbg Col Hit", PhysicsOffset(&PhysicsModifierDesc::debugCollisionHitCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugConstraintProjectionCount", "Dbg Project", PhysicsOffset(&PhysicsModifierDesc::debugConstraintProjectionCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugLocalClampCount", "Dbg Clamp", PhysicsOffset(&PhysicsModifierDesc::debugLocalClampCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugLocalClampXCount", "Dbg Clamp X", PhysicsOffset(&PhysicsModifierDesc::debugLocalClampXCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugLocalClampYCount", "Dbg Clamp Y", PhysicsOffset(&PhysicsModifierDesc::debugLocalClampYCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugLocalClampZCount", "Dbg Clamp Z", PhysicsOffset(&PhysicsModifierDesc::debugLocalClampZCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "debugInertiaMagnitude", "Dbg Inertia", PhysicsOffset(&PhysicsModifierDesc::debugInertiaMagnitude), 0.001f, 0.f, 100.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugTraceFrameCount", "Dbg Trace Frames", PhysicsOffset(&PhysicsModifierDesc::debugTraceFrameCount), 1.f, 0.f, 900.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "debugTraceMaxNodeMotion", "Dbg Trace Move", PhysicsOffset(&PhysicsModifierDesc::debugTraceMaxNodeMotion), 0.001f, 0.f, 100.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "debugTraceMaxOutputAngle", "Dbg Trace Angle", PhysicsOffset(&PhysicsModifierDesc::debugTraceMaxOutputAngle), 0.001f, 0.f, 180.f);

			AddRuntimeProperty(result.properties, PropertyType::Float, "outputAlpha", "Output", PhysicsOffset(&PhysicsModifierDesc::outputAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "rotationOutputAlpha", "Rot Out", PhysicsOffset(&PhysicsModifierDesc::rotationOutputAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Bool, "enableTranslationOutput", "Trans Out", PhysicsOffset(&PhysicsModifierDesc::enableTranslationOutput));
			AddRuntimeProperty(result.properties, PropertyType::Float, "rootRotationOutputScale", "Root Rot Out", PhysicsOffset(&PhysicsModifierDesc::rootRotationOutputScale), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "rotationOutputDepthStart", "Rot Depth Start", PhysicsOffset(&PhysicsModifierDesc::rotationOutputDepthStart), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "rotationOutputDepthEnd", "Rot Depth End", PhysicsOffset(&PhysicsModifierDesc::rotationOutputDepthEnd), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "outputSmoothAlpha", "Smooth", PhysicsOffset(&PhysicsModifierDesc::outputSmoothAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "idleOutputSmoothAlpha", "Idle Smooth", PhysicsOffset(&PhysicsModifierDesc::idleOutputSmoothAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "idleMotionDeadzone", "Idle Dead", PhysicsOffset(&PhysicsModifierDesc::idleMotionDeadzone), 0.001f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "maxOutputAngleDegree", "Max Angle", PhysicsOffset(&PhysicsModifierDesc::maxOutputAngleDegree), 0.25f, 0.f, 180.f);

			AddRuntimeProperty(result.properties, PropertyType::Float, "freeRotationFollowAlpha", "Free Follow", PhysicsOffset(&PhysicsModifierDesc::freeRotationFollowAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "rotationFollowDepthScale", "Depth Follow", PhysicsOffset(&PhysicsModifierDesc::rotationFollowDepthScale), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "poseRestoreAlpha", "Restore", PhysicsOffset(&PhysicsModifierDesc::poseRestoreAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "poseRestoreMaxSpeed", "Restore Speed", PhysicsOffset(&PhysicsModifierDesc::poseRestoreMaxSpeed), 0.05f, 0.f, 100.f);
			AddRuntimeProperty(result.properties, PropertyType::Vec3, "maxLocalOffset", "Max Local", PhysicsOffset(&PhysicsModifierDesc::maxLocalOffset), 0.005f);
			AddRuntimeProperty(result.properties, PropertyType::Bool, "enableJointLimit", "Joint Limit", PhysicsOffset(&PhysicsModifierDesc::enableJointLimit));
			AddRuntimeProperty(result.properties, PropertyType::Float, "movementInertiaScale", "Move Inertia", PhysicsOffset(&PhysicsModifierDesc::movementInertiaScale), 0.005f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "tipMovementInertiaScale", "Tip Inertia", PhysicsOffset(&PhysicsModifierDesc::tipMovementInertiaScale), 0.01f, 0.f, 30.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "maxMovementInertiaSpeed", "Max Inertia", PhysicsOffset(&PhysicsModifierDesc::maxMovementInertiaSpeed), 0.05f, 0.f, 80.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "lateralInertiaScale", "Side Inertia", PhysicsOffset(&PhysicsModifierDesc::lateralInertiaScale), 0.01f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "verticalInertiaScale", "Up Inertia", PhysicsOffset(&PhysicsModifierDesc::verticalInertiaScale), 0.01f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "depthInertiaScale", "Depth Inertia", PhysicsOffset(&PhysicsModifierDesc::depthInertiaScale), 0.01f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "ownerRotationInertiaScale", "Turn Inertia", PhysicsOffset(&PhysicsModifierDesc::ownerRotationInertiaScale), 0.01f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Bool, "includeDriverLateralInertia", "Driver Side", PhysicsOffset(&PhysicsModifierDesc::includeDriverLateralInertia));
			AddRuntimeProperty(result.properties, PropertyType::Bool, "useSteadyInertia", "Steady Inertia", PhysicsOffset(&PhysicsModifierDesc::useSteadyInertia));
			AddRuntimeProperty(result.properties, PropertyType::Bool, "advectSimulationWithAnchor", "Anchor Advect", PhysicsOffset(&PhysicsModifierDesc::advectSimulationWithAnchor));
			AddRuntimeProperty(result.properties, PropertyType::Float, "anchorAdvectionAlpha", "Advect Alpha", PhysicsOffset(&PhysicsModifierDesc::anchorAdvectionAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "accelerationInertiaScale", "Accel Inertia", PhysicsOffset(&PhysicsModifierDesc::accelerationInertiaScale), 0.01f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "inertiaVelocitySmoothAlpha", "Inertia Smooth", PhysicsOffset(&PhysicsModifierDesc::inertiaVelocitySmoothAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "poseSpringStrength", "Spring", PhysicsOffset(&PhysicsModifierDesc::poseSpringStrength), 0.01f, 0.f, 20.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "poseSpringDamping", "Spring Damp", PhysicsOffset(&PhysicsModifierDesc::poseSpringDamping), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "lengthConstraintAlpha", "Length Cst", PhysicsOffset(&PhysicsModifierDesc::lengthConstraintAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "bendConstraintAlpha", "Bend Cst", PhysicsOffset(&PhysicsModifierDesc::bendConstraintAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "projectionIterations", "Proj Iter", PhysicsOffset(&PhysicsModifierDesc::projectionIterations), 1.f, 1.f, 16.f);

			AddRuntimeProperty(result.properties, PropertyType::Float, "releaseInertiaDamping", "Release Damp", PhysicsOffset(&PhysicsModifierDesc::releaseInertiaDamping), 0.005f, 0.f, 1.f);

			AddRuntimeProperty(result.properties, PropertyType::Float, "collisionPrimitiveScale", "Col Scale", PhysicsOffset(&PhysicsModifierDesc::collisionPrimitiveScale), 0.005f, 0.001f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "collisionRadiusPadding", "Col Pad", PhysicsOffset(&PhysicsModifierDesc::collisionRadiusPadding), 0.005f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "collisionContactSkinWidth", "Col Skin", PhysicsOffset(&PhysicsModifierDesc::collisionContactSkinWidth), 0.005f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "collisionSphereRadius", "Node Radius", PhysicsOffset(&PhysicsModifierDesc::collisionSphereRadius), 0.005f, 0.001f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "collisionProjectionIterations", "Col Iter", PhysicsOffset(&PhysicsModifierDesc::collisionProjectionIterations), 1.f, 0.f, 32.f);
			AddRuntimeProperty(result.properties, PropertyType::Bool, "useHardCollisionProjection", "Hard Col", PhysicsOffset(&PhysicsModifierDesc::useHardCollisionProjection));
			AddRuntimeProperty(result.properties, PropertyType::Int, "hardCollisionProjectionIterations", "Hard Iter", PhysicsOffset(&PhysicsModifierDesc::hardCollisionProjectionIterations), 1.f, 0.f, 16.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "hardCollisionLengthAlpha", "Hard Len", PhysicsOffset(&PhysicsModifierDesc::hardCollisionLengthAlpha), 0.005f, 0.f, 1.f);

			AddRuntimeProperty(result.properties, PropertyType::Vec3, "windDirection", "Wind Dir", PhysicsOffset(&PhysicsModifierDesc::windDirection), 0.01f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "windScale", "Wind", PhysicsOffset(&PhysicsModifierDesc::windScale), 0.001f, 0.f, 100.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "windWaveScale", "Wind Wave", PhysicsOffset(&PhysicsModifierDesc::windWaveScale), 0.005f, 0.f, 2.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "windWaveFrequency", "Wind Freq", PhysicsOffset(&PhysicsModifierDesc::windWaveFrequency), 0.01f, 0.f, 20.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "windWaveDepthPhase", "Wind Phase", PhysicsOffset(&PhysicsModifierDesc::windWaveDepthPhase), 0.01f, -10.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "gravityScale", "Gravity", PhysicsOffset(&PhysicsModifierDesc::gravityScale), 0.01f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Bool, "useHierarchicalDepthRatio", "Tree Depth", PhysicsOffset(&PhysicsModifierDesc::useHierarchicalDepthRatio));
			AddRuntimeProperty(result.properties, PropertyType::Float, "teleportDistance", "Reset Dist", PhysicsOffset(&PhysicsModifierDesc::teleportDistance), 0.05f, 0.f, 1000.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "solverIterations", "Solver Iter", PhysicsOffset(&PhysicsModifierDesc::solverIterations), 1.f, 1.f, 64.f);

			return result;
		}();

	return &info;
}

void PhysicsModifier::On_RuntimeReflectionChanged(ModelCom* model, const PropertyInfo& property)
{
	if (property.name.rfind("debug", 0) == 0)
		return;

	RuntimeRebuild(model);
}

NS_END


