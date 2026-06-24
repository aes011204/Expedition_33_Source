#include "PhysicsModifier.h"
#include "ModelCom.h"
#include "ModelPhysicsParser.h"
#include "Reflection_Types.h"
#include "GameInstance.h"
#include "Engine_Macro.h"
#include "Collision_Define.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

NS_BEGIN(Engine)

namespace
{
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

	Matrix Get_OwnerWorld(const PhysicsModifier::PhysicsModifierDesc& desc)
	{
		if (desc.ownerWorldMatrix)
		{
			return *desc.ownerWorldMatrix;
		}

		return desc.ownerTransform ? desc.ownerTransform->Get_WorldMatrix() : Matrix::Identity;
	}

	Vec3 Get_Position(const Matrix& matrix)
	{
		return Vec3(matrix._41, matrix._42, matrix._43);
	}

	float SmoothStep(float value)
	{
		value = std::clamp(value, 0.f, 1.f);
		return value * value * (3.f - 2.f * value);
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

	Vec3 Limit_LocalDirectionByAngle(const Vec3& poseLocalDirection, const Vec3& targetLocalDirection, float maxAngleDegree)
	{
		if (maxAngleDegree <= 0.f)
		{
			return targetLocalDirection;
		}

		Vec3 poseDir = poseLocalDirection;
		Vec3 targetDir = targetLocalDirection;
		if (poseDir.LengthSquared() <= 0.0001f || targetDir.LengthSquared() <= 0.0001f)
		{
			return poseLocalDirection;
		}

		poseDir.Normalize();
		targetDir.Normalize();
		const float maxAngle = XMConvertToRadians(maxAngleDegree);
		const float dot = std::clamp(poseDir.Dot(targetDir), -1.f, 1.f);
		const float angle = acosf(dot);
		if (angle <= maxAngle)
		{
			return targetDir;
		}

		Vec3 axis = poseDir.Cross(targetDir);
		if (axis.LengthSquared() <= 0.0001f)
		{
			return poseDir;
		}
		axis.Normalize();

		Vec3 limitedDir = Vec3::Transform(poseDir, Matrix::CreateFromQuaternion(Quat::CreateFromAxisAngle(axis, maxAngle)));
		if (limitedDir.LengthSquared() > 0.0001f)
		{
			limitedDir.Normalize();
		}
		return limitedDir;
	}

	Quat Build_RotationDelta(const Vec3& fromDirection, const Vec3& toDirection)
	{
		Vec3 from = fromDirection;
		Vec3 to = toDirection;
		if (from.LengthSquared() <= 0.0001f || to.LengthSquared() <= 0.0001f)
		{
			return Quat::Identity;
		}

		from.Normalize();
		to.Normalize();

		const float dot = std::clamp(from.Dot(to), -1.f, 1.f);
		if (dot > 0.9995f)
		{
			return Quat::Identity;
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
				return Quat::Identity;
			}

			axis.Normalize();
			return Quat::CreateFromAxisAngle(axis, XM_PI);
		}

		Vec3 axis = from.Cross(to);
		if (axis.LengthSquared() <= 0.0001f)
		{
			return Quat::Identity;
		}

		axis.Normalize();
		Quat delta = Quat::CreateFromAxisAngle(axis, acosf(dot));
		delta.Normalize();
		return delta;
	}

	bool Build_SegmentWorldFrame(const Vec3& start, const Vec3& end, Matrix& outFrame)
	{
		Vec3 segmentAxis = end - start;
		const float segmentLength = segmentAxis.Length();
		if (segmentLength <= 0.0001f)
		{
			return false;
		}

		segmentAxis /= segmentLength;
		Quat rotation = Build_RotationDelta(Vec3::UnitX, segmentAxis);
		rotation.Normalize();

		const Vec3 center = (start + end) * 0.5f;
		outFrame = Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(center);
		return true;
	}

	Matrix Build_LocalMatrix_FromSRT(const Vec3& scale, const Quat& rotation, const Vec3& translation)
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

	Vec3 Clamp_LocalTranslationFromPose(
		const Vec3& poseTranslation,
		const Vec3& targetTranslation,
		float maxDelta)
	{
		Vec3 delta = targetTranslation - poseTranslation;
		const float deltaLength = delta.Length();
		if (deltaLength <= 0.000001f)
		{
			return poseTranslation;
		}

		const float limitedDelta = std::max(0.f, maxDelta);
		if (limitedDelta <= 0.f)
		{
			return poseTranslation;
		}

		if (deltaLength > limitedDelta)
		{
			delta *= limitedDelta / deltaLength;
		}

		return poseTranslation + delta;
	}

	Vec3 Clamp_VectorLength(const Vec3& value, float maxLength)
	{
		const float length = value.Length();
		const float limit = std::max(0.f, maxLength);
		if (length <= 0.000001f || limit <= 0.f || length <= limit)
		{
			return value;
		}

		return value * (limit / length);
	}

	Vec3 Resolve_PushDirection(
		Vec3 normal,
		Vec3 preferredDirection,
		float preferredBlend)
	{
		if (normal.LengthSquared() <= 0.0001f)
		{
			normal = Vec3::Up;
		}
		normal.Normalize();

		preferredBlend = std::clamp(preferredBlend, 0.f, 1.f);
		if (preferredBlend <= 0.0001f || preferredDirection.LengthSquared() <= 0.0001f)
		{
			return normal;
		}

		preferredDirection.Normalize();
		if (preferredDirection.Dot(normal) < 0.f)
		{
			preferredDirection *= -1.f;
		}

		Vec3 blended = Vec3::Lerp(normal, preferredDirection, preferredBlend);
		if (blended.LengthSquared() <= 0.0001f)
		{
			return normal;
		}

		blended.Normalize();
		return blended;
	}

	float SegmentSegmentClosestPoints(const Vec3& p1, const Vec3& q1, const Vec3& p2, const Vec3& q2, Vec3& outC1, Vec3& outC2)
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
			return (p1 - p2).LengthSquared();
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
				{
					s = std::clamp((b * f - c * e) / denom, 0.f, 1.f);
				}

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
		return (outC1 - outC2).LengthSquared();
	}


	Vec3 ClosestPointOnSegment(const Vec3& point, const Vec3& start, const Vec3& end)
	{
		const Vec3 segment = end - start;
		const float segmentLengthSq = segment.LengthSquared();
		if (segmentLengthSq <= 0.000001f)
		{
			return start;
		}

		const float t = std::clamp((point - start).Dot(segment) / segmentLengthSq, 0.f, 1.f);
		return start + segment * t;
	}

	Vec3 BuildEndpointCollisionCorrection(
		const Vec3& segmentStart,
		const Vec3& segmentEnd,
		const Vec3& segmentClosest,
		const Vec3& capsuleClosest,
		const Vec3& capsuleStart,
		const Vec3& capsuleEnd,
		float penetration,
		const Vec3& preferredDirection,
		float preferredBlend)
	{
		Vec3 normal = segmentClosest - capsuleClosest;
		if (normal.LengthSquared() <= 0.0001f)
		{
			const Vec3 endpointCapsuleClosest = ClosestPointOnSegment(segmentEnd, capsuleStart, capsuleEnd);
			normal = segmentEnd - endpointCapsuleClosest;
		}
		if (normal.LengthSquared() <= 0.0001f)
		{
			const Vec3 startCapsuleClosest = ClosestPointOnSegment(segmentStart, capsuleStart, capsuleEnd);
			normal = segmentStart - startCapsuleClosest;
		}
		if (normal.LengthSquared() <= 0.0001f)
		{
			normal = segmentEnd - segmentStart;
		}
		if (normal.LengthSquared() <= 0.0001f)
		{
			normal = Vec3::Up;
		}
		normal = Resolve_PushDirection(normal, preferredDirection, preferredBlend);

		const Vec3 segment = segmentEnd - segmentStart;
		float endpointScale = 1.f;
		const float segmentLengthSq = segment.LengthSquared();
		if (segmentLengthSq > 0.000001f)
		{
			const float contactT = std::clamp((segmentClosest - segmentStart).Dot(segment) / segmentLengthSq, 0.f, 1.f);
			endpointScale = 1.f / std::max(contactT, 0.20f);
		}

		return normal * penetration * std::min(endpointScale, 5.f);
	}

	bool BuildSegmentCollisionCorrections(
		const Vec3& segmentStart,
		const Vec3& segmentEnd,
		const Vec3& segmentClosest,
		const Vec3& capsuleClosest,
		const Vec3& capsuleStart,
		const Vec3& capsuleEnd,
		float penetration,
		float scale,
		float maxCorrection,
		bool canMoveStart,
		const Vec3& preferredDirection,
		float preferredBlend,
		Vec3& outStartCorrection,
		Vec3& outEndCorrection)
	{
		outStartCorrection = Vec3::Zero;
		outEndCorrection = Vec3::Zero;
		if (penetration <= 0.f || scale <= 0.f)
		{
			return false;
		}

		Vec3 normal = segmentClosest - capsuleClosest;
		if (normal.LengthSquared() <= 0.0001f)
		{
			const Vec3 endpointCapsuleClosest = ClosestPointOnSegment(segmentEnd, capsuleStart, capsuleEnd);
			normal = segmentEnd - endpointCapsuleClosest;
		}
		if (normal.LengthSquared() <= 0.0001f)
		{
			const Vec3 startCapsuleClosest = ClosestPointOnSegment(segmentStart, capsuleStart, capsuleEnd);
			normal = segmentStart - startCapsuleClosest;
		}
		if (normal.LengthSquared() <= 0.0001f)
		{
			normal = segmentEnd - segmentStart;
		}
		if (normal.LengthSquared() <= 0.0001f)
		{
			normal = Vec3::Up;
		}
		normal = Resolve_PushDirection(normal, preferredDirection, preferredBlend);

		float contactT = 1.f;
		const Vec3 segment = segmentEnd - segmentStart;
		const float segmentLengthSq = segment.LengthSquared();
		if (segmentLengthSq > 0.000001f)
		{
			contactT = std::clamp((segmentClosest - segmentStart).Dot(segment) / segmentLengthSq, 0.f, 1.f);
		}

		float startWeight = canMoveStart ? 1.f - contactT : 0.f;
		float endWeight = contactT;
		if (!canMoveStart)
		{
			endWeight = std::max(endWeight, 0.20f);
		}

		float denom = startWeight * startWeight + endWeight * endWeight;
		if (denom <= 0.0001f)
		{
			endWeight = 1.f;
			denom = 1.f;
		}

		const float lambda = penetration * scale / denom;
		outStartCorrection = normal * (lambda * startWeight);
		outEndCorrection = normal * (lambda * endWeight);

		const float maxDelta = std::max(0.001f, maxCorrection);
		const float startLength = outStartCorrection.Length();
		if (startLength > maxDelta)
		{
			outStartCorrection *= maxDelta / startLength;
		}
		const float endLength = outEndCorrection.Length();
		if (endLength > maxDelta)
		{
			outEndCorrection *= maxDelta / endLength;
		}

		return outStartCorrection.LengthSquared() > 0.00000001f || outEndCorrection.LengthSquared() > 0.00000001f;
	}

	bool BuildHardSegmentCollisionCorrections(
		const Vec3& segmentStart,
		const Vec3& segmentEnd,
		const Vec3& segmentClosest,
		const Vec3& capsuleClosest,
		const Vec3& capsuleStart,
		const Vec3& capsuleEnd,
		float penetration,
		bool canMoveStart,
		const Vec3& preferredDirection,
		float preferredBlend,
		Vec3& outStartCorrection,
		Vec3& outEndCorrection)
	{
		outStartCorrection = Vec3::Zero;
		outEndCorrection = Vec3::Zero;
		if (penetration <= 0.f)
		{
			return false;
		}

		Vec3 normal = segmentClosest - capsuleClosest;
		if (normal.LengthSquared() <= 0.0001f)
		{
			const Vec3 endpointCapsuleClosest = ClosestPointOnSegment(segmentEnd, capsuleStart, capsuleEnd);
			normal = segmentEnd - endpointCapsuleClosest;
		}
		if (normal.LengthSquared() <= 0.0001f)
		{
			const Vec3 startCapsuleClosest = ClosestPointOnSegment(segmentStart, capsuleStart, capsuleEnd);
			normal = segmentStart - startCapsuleClosest;
		}
		if (normal.LengthSquared() <= 0.0001f)
		{
			normal = segmentEnd - segmentStart;
		}
		if (normal.LengthSquared() <= 0.0001f)
		{
			normal = Vec3::Up;
		}
		normal = Resolve_PushDirection(normal, preferredDirection, preferredBlend);

		float contactT = 1.f;
		const Vec3 segment = segmentEnd - segmentStart;
		const float segmentLengthSq = segment.LengthSquared();
		if (segmentLengthSq > 0.000001f)
		{
			contactT = std::clamp((segmentClosest - segmentStart).Dot(segment) / segmentLengthSq, 0.f, 1.f);
		}

		float startWeight = canMoveStart ? 1.f - contactT : 0.f;
		float endWeight = contactT;
		if (!canMoveStart)
		{
			endWeight = std::max(endWeight, 0.20f);
		}

		float denom = startWeight * startWeight + endWeight * endWeight;
		if (denom <= 0.0001f)
		{
			endWeight = 1.f;
			denom = 1.f;
		}

		const float lambda = penetration / denom;
		outStartCorrection = normal * (lambda * startWeight);
		outEndCorrection = normal * (lambda * endWeight);
		return outStartCorrection.LengthSquared() > 0.00000001f || outEndCorrection.LengthSquared() > 0.00000001f;
	}

	bool BuildCollisionDirectionBlockPosition(
		const Vec3& segmentStart,
		const Vec3& segmentEnd,
		const Vec3& segmentClosest,
		const Vec3& capsuleClosest,
		const Vec3& capsuleStart,
		const Vec3& capsuleEnd,
		float targetLength,
		float radius,
		float skinWidth,
		float alpha,
		const Vec3& preferredDirection,
		float preferredBlend,
		Vec3& outPosition)
	{
		alpha = std::clamp(alpha, 0.f, 1.f);
		if (alpha <= 0.0001f || targetLength <= 0.0001f)
		{
			return false;
		}

		Vec3 normal = segmentClosest - capsuleClosest;
		if (normal.LengthSquared() <= 0.0001f)
		{
			const Vec3 endpointCapsuleClosest = ClosestPointOnSegment(segmentEnd, capsuleStart, capsuleEnd);
			normal = segmentEnd - endpointCapsuleClosest;
		}
		if (normal.LengthSquared() <= 0.0001f)
		{
			const Vec3 startCapsuleClosest = ClosestPointOnSegment(segmentStart, capsuleStart, capsuleEnd);
			normal = segmentStart - startCapsuleClosest;
		}
		if (normal.LengthSquared() <= 0.0001f)
		{
			return false;
		}
		normal = Resolve_PushDirection(normal, preferredDirection, preferredBlend);

		Vec3 direction = segmentEnd - segmentStart;
		if (direction.LengthSquared() <= 0.000001f)
		{
			return false;
		}
		direction.Normalize();

		const float inward = direction.Dot(normal);
		if (inward >= 0.f)
		{
			return false;
		}

		Vec3 blockedDirection = direction - normal * inward;
		if (blockedDirection.LengthSquared() <= 0.0001f)
		{
			blockedDirection = normal;
		}
		else
		{
			blockedDirection.Normalize();
			blockedDirection += normal * 0.20f;
			blockedDirection.Normalize();
		}

		Vec3 blockedPosition = segmentStart + blockedDirection * targetLength;
		const Vec3 blockedClosest = ClosestPointOnSegment(blockedPosition, capsuleStart, capsuleEnd);
		const Vec3 fromCapsule = blockedPosition - blockedClosest;
		const float minDistance = std::max(0.f, radius + skinWidth);
		if (fromCapsule.LengthSquared() > 0.0001f && fromCapsule.Length() < minDistance)
		{
			Vec3 endpointNormal = fromCapsule;
			endpointNormal.Normalize();
			blockedPosition = blockedClosest + endpointNormal * minDistance;
			Vec3 finalDirection = blockedPosition - segmentStart;
			if (finalDirection.LengthSquared() > 0.0001f)
			{
				finalDirection.Normalize();
				blockedPosition = segmentStart + finalDirection * targetLength;
			}
		}

		outPosition = Vec3::Lerp(segmentEnd, blockedPosition, alpha);
		return true;
	}

}

HRESULT PhysicsModifier::Initialize(AnimationModifierDesc* desc)
{
	if (FAILED(__super::Initialize(desc)))
	{
		return E_FAIL;
	}

	auto* physicsDesc = static_cast<PhysicsModifierDesc*>(desc);
	if (!physicsDesc)
	{
		return E_FAIL;
	}
	_desc = *physicsDesc;
	_colliders = _desc.overrideCapsules.empty()
		? ModelPhysicsParser::LoadCapsulesFromJSON(_desc.physicsAssetPath)
		: _desc.overrideCapsules;
	_desc.debugLoadedCapsuleCount = static_cast<int32>(_colliders.size());
	_desc.debugActiveChainCount = 0;
	_desc.debugActiveNodeCount = 0;
	_desc.debugActiveCapsuleCount = 0;
	_desc.debugCollisionSource = _desc.collisionModel ? 1 : 0;

	_configuredRoots.clear();
	for (const auto& chainDesc : _desc.chains)
	{
		if (!chainDesc.rootBoneName.empty())
		{
			_configuredRoots.insert(chainDesc.rootBoneName);
		}
	}

	_chainsInitialized = false;
	_isFirstUpdate = true;
	return S_OK;
}

void PhysicsModifier::On_ActiveChanged(ModelCom* model, bool isActive)
{
	if (isActive)
	{
		ResetPose(model);
		return;
	}

	Restore_PoseLocals(model);
}

void PhysicsModifier::RuntimeRebuild(ModelCom* model)
{
	_chainsInitialized = false;
	Initialize_Chains(model);
	ResetPose(model);
}

void PhysicsModifier::Set_RuntimeTestOwnerMotion(bool enabled, const Vec3& delta)
{
	_runtimeTestOwnerMotionEnabled = enabled;
	_runtimeTestOwnerMotionDelta = enabled ? delta : Vec3::Zero;
}

void PhysicsModifier::Build_RuntimeDebugCapsules(ModelCom* model, vector<RuntimeDebugCapsule>& outCapsules) const
{
	outCapsules.clear();
	if (!model || !_desc.enableCollision)
	{
		_desc.debugActiveCapsuleCount = 0;
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
		debugCapsule.collisionRadius = GetCollisionContactDistance(capsule);
		outCapsules.push_back(debugCapsule);
	}
}

void PhysicsModifier::Build_RuntimeDebugSegments(vector<RuntimeDebugSegment>& outSegments) const
{
	outSegments.clear();
	_desc.debugSegmentCount = 0;
	for (const auto& chain : _chains)
	{
		for (const auto& node : chain.nodes)
		{
			RuntimeDebugSegment debugSegment{};
			if (node.parentNodeIndex >= 0)
			{
				debugSegment.startPoint = chain.nodes[static_cast<size_t>(node.parentNodeIndex)].position;
				debugSegment.endPoint = node.position;
				debugSegment.hasContact = node.hasCollisionContact || node.hasCollisionPushApplied;
				debugSegment.hasPush = node.hasCollisionPushApplied;
				debugSegment.isSolvedSegment = true;
				outSegments.push_back(debugSegment);
				++_desc.debugSegmentCount;
			}
			else if (chain.anchorBoneIndex >= 0)
			{
				debugSegment.startPoint = Get_Position(chain.anchorWorld);
				debugSegment.endPoint = node.position;
				debugSegment.hasContact = node.hasCollisionContact || node.hasCollisionPushApplied;
				debugSegment.hasPush = node.hasCollisionPushApplied;
				debugSegment.isSolvedSegment = true;
				outSegments.push_back(debugSegment);
				++_desc.debugSegmentCount;
			}
		}
	}
}

float PhysicsModifier::GetCollisionQueryRadius(const WorldCapsule& capsule) const
{
	return std::max(0.f, capsule.radius)
		+ std::max(0.f, _desc.collisionRadiusPadding)
		+ std::max(0.f, _desc.hairCollisionClearance);
}

float PhysicsModifier::GetCollisionContactDistance(const WorldCapsule& capsule) const
{
	return GetCollisionQueryRadius(capsule)
		+ std::max(0.f, _desc.collisionContactSkinWidth);
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

			AddRuntimeProperty(result.properties, PropertyType::Bool, "writeBackEnabled", "Write", PhysicsOffset(&PhysicsModifierDesc::writeBackEnabled));
			AddRuntimeProperty(result.properties, PropertyType::Bool, "enableCollision", "Collision", PhysicsOffset(&PhysicsModifierDesc::enableCollision));
			AddRuntimeProperty(result.properties, PropertyType::Bool, "debugDrawChains", "Debug Chain", PhysicsOffset(&PhysicsModifierDesc::debugDrawChains));
			AddRuntimeProperty(result.properties, PropertyType::Bool, "debugDrawNodes", "Debug Node", PhysicsOffset(&PhysicsModifierDesc::debugDrawNodes));
			AddRuntimeProperty(result.properties, PropertyType::Bool, "debugDrawSegments", "Debug Seg", PhysicsOffset(&PhysicsModifierDesc::debugDrawSegments));
			AddRuntimeProperty(result.properties, PropertyType::Bool, "debugDrawCollisions", "Debug Col", PhysicsOffset(&PhysicsModifierDesc::debugDrawCollisions));
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugActiveChainCount", "Active Chain", PhysicsOffset(&PhysicsModifierDesc::debugActiveChainCount), 1.f, 0.f, 128.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugActiveNodeCount", "Active Node", PhysicsOffset(&PhysicsModifierDesc::debugActiveNodeCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugLoadedCapsuleCount", "Loaded Col", PhysicsOffset(&PhysicsModifierDesc::debugLoadedCapsuleCount), 1.f, 0.f, 64.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugActiveCapsuleCount", "Active Col", PhysicsOffset(&PhysicsModifierDesc::debugActiveCapsuleCount), 1.f, 0.f, 64.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugCollisionSource", "Col Source", PhysicsOffset(&PhysicsModifierDesc::debugCollisionSource), 1.f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugCollisionHitCount", "Hit Col", PhysicsOffset(&PhysicsModifierDesc::debugCollisionHitCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugSegmentCollisionHitCount", "Hit Seg", PhysicsOffset(&PhysicsModifierDesc::debugSegmentCollisionHitCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "debugMinCollisionMargin", "Min Margin", PhysicsOffset(&PhysicsModifierDesc::debugMinCollisionMargin), 0.01f, -100.f, 100.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugCollisionPushOutCount", "Push Seg", PhysicsOffset(&PhysicsModifierDesc::debugCollisionPushOutCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "debugMaxCollisionPushOut", "Push Dist", PhysicsOffset(&PhysicsModifierDesc::debugMaxCollisionPushOut), 0.01f, 0.f, 100.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugSegmentCount", "Node Seg", PhysicsOffset(&PhysicsModifierDesc::debugSegmentCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "collisionPrimitiveScale", "Col Scale", PhysicsOffset(&PhysicsModifierDesc::collisionPrimitiveScale), 0.001f, 0.001f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "collisionRadiusPadding", "Col Pad", PhysicsOffset(&PhysicsModifierDesc::collisionRadiusPadding), 0.01f, 0.f, 100.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "collisionPushOutScale", "Contact Push", PhysicsOffset(&PhysicsModifierDesc::collisionPushOutScale), 0.01f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "collisionPushOutMaxScale", "Contact Max", PhysicsOffset(&PhysicsModifierDesc::collisionPushOutMaxScale), 0.01f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "collisionContactSkinWidth", "Contact Skin", PhysicsOffset(&PhysicsModifierDesc::collisionContactSkinWidth), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Vec3, "collisionPushDirectionWorld", "Push Dir", PhysicsOffset(&PhysicsModifierDesc::collisionPushDirectionWorld), 0.01f, -1.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "collisionPushDirectionBlend", "Push Dir Blend", PhysicsOffset(&PhysicsModifierDesc::collisionPushDirectionBlend), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "collisionDirectionSmoothAlpha", "Dir Smooth", PhysicsOffset(&PhysicsModifierDesc::collisionDirectionSmoothAlpha), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "contactOutputAlpha", "Contact Out", PhysicsOffset(&PhysicsModifierDesc::contactOutputAlpha), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "contactOutputSmoothAlpha", "Contact Smooth", PhysicsOffset(&PhysicsModifierDesc::contactOutputSmoothAlpha), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "contactOutputLimitDegree", "Contact Limit", PhysicsOffset(&PhysicsModifierDesc::contactOutputLimitDegree), 1.f, 0.f, 180.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "freePoseFollowAlpha", "Free Follow", PhysicsOffset(&PhysicsModifierDesc::freePoseFollowAlpha), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "contactPoseFollowAlpha", "Contact Follow", PhysicsOffset(&PhysicsModifierDesc::contactPoseFollowAlpha), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "releasePoseFollowAlpha", "Release Follow", PhysicsOffset(&PhysicsModifierDesc::releasePoseFollowAlpha), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "releaseMemoryTime", "Release Time", PhysicsOffset(&PhysicsModifierDesc::releaseMemoryTime), 0.01f, 0.f, 2.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "contactSolveLimitDegree", "Solve Limit", PhysicsOffset(&PhysicsModifierDesc::contactSolveLimitDegree), 1.f, 0.f, 180.f);
			AddRuntimeProperty(result.properties, PropertyType::Bool, "segmentFrameOutput", "Seg Frame", PhysicsOffset(&PhysicsModifierDesc::segmentFrameOutput));
			AddRuntimeProperty(result.properties, PropertyType::Float, "rotationOutputAlpha", "Rot Out", PhysicsOffset(&PhysicsModifierDesc::rotationOutputAlpha), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "translationOutputAlpha", "Trans Out", PhysicsOffset(&PhysicsModifierDesc::translationOutputAlpha), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "contactCorrectionSmoothAlpha", "Col Smooth", PhysicsOffset(&PhysicsModifierDesc::contactCorrectionSmoothAlpha), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "contactCorrectionReleaseAlpha", "Col Release", PhysicsOffset(&PhysicsModifierDesc::contactCorrectionReleaseAlpha), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "childCollisionPushDamping", "Child Damp", PhysicsOffset(&PhysicsModifierDesc::childCollisionPushDamping), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "tipCollisionPushDamping", "Tip Push", PhysicsOffset(&PhysicsModifierDesc::tipCollisionPushDamping), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "contactBendPropagationAlpha", "Bend Push", PhysicsOffset(&PhysicsModifierDesc::contactBendPropagationAlpha), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "contactBendPropagationDecay", "Bend Decay", PhysicsOffset(&PhysicsModifierDesc::contactBendPropagationDecay), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "finalCollisionProjectionScale", "Final Push", PhysicsOffset(&PhysicsModifierDesc::finalCollisionProjectionScale), 0.01f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "finalCollisionProjectionMaxScale", "Final Max", PhysicsOffset(&PhysicsModifierDesc::finalCollisionProjectionMaxScale), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "finalCollisionProjectionIterations", "Final Iter", PhysicsOffset(&PhysicsModifierDesc::finalCollisionProjectionIterations), 1.f, 1.f, 8.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "collisionDirectionBlockAlpha", "Dir Block", PhysicsOffset(&PhysicsModifierDesc::collisionDirectionBlockAlpha), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "contactSnapAlpha", "Contact Snap", PhysicsOffset(&PhysicsModifierDesc::contactSnapAlpha), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "releaseContactCorrectionAlpha", "Release Col", PhysicsOffset(&PhysicsModifierDesc::releaseContactCorrectionAlpha), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "hairCollisionClearance", "Hair Clear", PhysicsOffset(&PhysicsModifierDesc::hairCollisionClearance), 0.005f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "outputAlpha", "Output", PhysicsOffset(&PhysicsModifierDesc::outputAlpha), 0.005f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "outputSmoothAlpha", "Smooth", PhysicsOffset(&PhysicsModifierDesc::outputSmoothAlpha), 0.001f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "idleOutputSmoothAlpha", "Idle Smooth", PhysicsOffset(&PhysicsModifierDesc::idleOutputSmoothAlpha), 0.001f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "tipOutputSmoothScale", "Tip Smooth", PhysicsOffset(&PhysicsModifierDesc::tipOutputSmoothScale), 0.001f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "inertiaDamping", "Inertia Damp", PhysicsOffset(&PhysicsModifierDesc::inertiaDamping), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "contactInertiaDamping", "Hit Damp", PhysicsOffset(&PhysicsModifierDesc::contactInertiaDamping), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "releaseInertiaDamping", "Release Damp", PhysicsOffset(&PhysicsModifierDesc::releaseInertiaDamping), 0.01f, 0.f, 1.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "animTranslationInertiaScale", "Anim Move I", PhysicsOffset(&PhysicsModifierDesc::animTranslationInertiaScale), 0.01f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "animRotationInertiaScale", "Anim Rot I", PhysicsOffset(&PhysicsModifierDesc::animRotationInertiaScale), 0.01f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "ownerTranslationInertiaScale", "Owner Move I", PhysicsOffset(&PhysicsModifierDesc::ownerTranslationInertiaScale), 0.01f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "ownerRotationInertiaScale", "Owner Rot I", PhysicsOffset(&PhysicsModifierDesc::ownerRotationInertiaScale), 0.01f, 0.f, 5.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "inertiaMaxDistance", "Inertia Max", PhysicsOffset(&PhysicsModifierDesc::inertiaMaxDistance), 0.1f, 0.f, 100.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "debugInertiaMagnitude", "Inertia Now", PhysicsOffset(&PhysicsModifierDesc::debugInertiaMagnitude), 0.01f, 0.f, 100.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugContactNodeCount", "Contact Node", PhysicsOffset(&PhysicsModifierDesc::debugContactNodeCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "debugReleaseNodeCount", "Release Node", PhysicsOffset(&PhysicsModifierDesc::debugReleaseNodeCount), 1.f, 0.f, 4096.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "idleMotionDeadzone", "Idle Dead", PhysicsOffset(&PhysicsModifierDesc::idleMotionDeadzone), 0.001f, 0.f, 10.f);
			AddRuntimeProperty(result.properties, PropertyType::Int, "solverIterations", "Solver Iter", PhysicsOffset(&PhysicsModifierDesc::solverIterations), 1.f, 1.f, 32.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "targetFrameRate", "Target FPS", PhysicsOffset(&PhysicsModifierDesc::targetFrameRate), 1.f, 0.f, 240.f);
			AddRuntimeProperty(result.properties, PropertyType::Float, "teleportDistance", "Reset Dist", PhysicsOffset(&PhysicsModifierDesc::teleportDistance), 1.f, 0.f, 1000.f);

			return result;
		}();

	return &info;
}

void PhysicsModifier::On_RuntimeReflectionChanged(ModelCom* model, const PropertyInfo& property)
{
	UNREFERENCED_PARAMETER(property);

	if (!model || !_chainsInitialized)
	{
		return;
	}

	Restore_PoseLocals(model);
	model->Rebuild_BoneCombinedMatrices();
	const Matrix ownerWorld = Get_OwnerWorld(_desc);
	Update_ResolvedCollisionPushDirection(ownerWorld);
	Capture_CurrentPose(model, ownerWorld);
	Reset_SolvedStateToPose();
	Write_BackSolvedPoseToModel(model, ownerWorld, true);
	model->Rebuild_BoneCombinedMatrices();
}

void PhysicsModifier::ResetPose(ModelCom* model)
{
	if (!model)
	{
		return;
	}

	if (!_chainsInitialized)
	{
		Initialize_Chains(model);
	}
	else
	{
		Restore_PoseLocals(model);
	}

	model->Rebuild_BoneCombinedMatrices();
	const Matrix ownerWorld = Get_OwnerWorld(_desc);
	Update_ResolvedCollisionPushDirection(ownerWorld);
	Capture_CurrentPose(model, ownerWorld);
	Reset_SolvedStateToPose();

	_prevOwnerPos = Get_Position(ownerWorld);
	_prevOwnerWorld = ownerWorld;
	_isFirstUpdate = false;
}

void PhysicsModifier::Restore_PoseLocals(ModelCom* model)
{
	if (!model || !_chainsInitialized)
	{
		return;
	}

	for (const auto& chain : _chains)
	{
		for (const auto& node : chain.nodes)
		{
			model->Set_BoneLocalMatrix(node.boneIndex, node.bindLocal);
		}
	}
}

void PhysicsModifier::Reset_SolvedStateToPose()
{
	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			const Vec3 posePosition = Get_Position(node.originalWorld);
			node.position = posePosition;
			node.solvedPosition = posePosition;
			node.prevSolvedPosition = posePosition;
			node.prevPosePosition = posePosition;
			node.prevPoseDir = node.poseLocalDir;
			node.inertiaOffsetWorld = Vec3::Zero;
			node.hasSolvedState = true;
			node.hasPrevPoseState = true;
			node.contactCorrection = Vec3::Zero;
			node.smoothedContactCorrection = Vec3::Zero;
			node.smoothedCollisionNormal = Vec3::Zero;
			node.collisionCorrectionDistance = 0.f;
			node.contactReleaseTimer = std::max(0.f, _desc.releaseMemoryTime);
			node.wasCollisionDriven = false;
			node.hasCollisionContact = false;
			node.hasCollisionPushApplied = false;
			node.hasSmoothedCollisionNormal = false;
		}
	}
}

void PhysicsModifier::Initialize_Chains(ModelCom* model)
{
	_chains.clear();
	_chainsInitialized = true;
	_desc.debugActiveChainCount = 0;
	_desc.debugActiveNodeCount = 0;
	if (!model || !model->Is_Skeletal())
	{
		return;
	}

	for (const auto& chainDesc : _desc.chains)
	{
		const int32 rootBoneIndex = model->Get_BoneIndex(chainDesc.rootBoneName);
		if (rootBoneIndex < 0)
		{
			continue;
		}

		Chain chain{};
		chain.desc = chainDesc;
		chain.anchorBoneIndex = model->Get_BoneParentIndex(static_cast<uint32>(rootBoneIndex));
		BuildChainRecursive(model, rootBoneIndex, -1, chain);
		if (chain.nodes.size() >= 2)
		{
			vector<int32> nodeDepths(chain.nodes.size(), 0);
			int32 maxDepth = 0;
			for (size_t i = 0; i < chain.nodes.size(); ++i)
			{
				const int32 parentIndex = chain.nodes[i].parentNodeIndex;
				nodeDepths[i] = parentIndex >= 0 ? nodeDepths[static_cast<size_t>(parentIndex)] + 1 : 0;
				maxDepth = std::max(maxDepth, nodeDepths[i]);
			}
			for (size_t i = 0; i < chain.nodes.size(); ++i)
			{
				chain.nodes[i].depthRatio = maxDepth > 0
					? static_cast<float>(nodeDepths[i]) / static_cast<float>(maxDepth)
					: 0.f;
			}
			_chains.push_back(std::move(chain));
			_desc.debugActiveChainCount = static_cast<int32>(_chains.size());
			_desc.debugActiveNodeCount += static_cast<int32>(_chains.back().nodes.size());
		}
	}
}

void PhysicsModifier::BuildChainRecursive(ModelCom* model, int32 boneIndex, int32 parentNodeIndex, Chain& chain)
{
	if (!model || boneIndex < 0)
	{
		return;
	}

	const string& boneName = model->Get_BoneName(static_cast<uint32>(boneIndex));
	if (parentNodeIndex >= 0 && _configuredRoots.find(boneName) != _configuredRoots.end())
	{
		return;
	}

	Node node{};
	node.boneIndex = static_cast<uint32>(boneIndex);
	node.parentNodeIndex = parentNodeIndex;
	node.bindLocal = model->Get_BoneLocalMatrix(static_cast<uint32>(boneIndex));
	node.poseLocal = node.bindLocal;

	const int32 nodeIndex = static_cast<int32>(chain.nodes.size());
	chain.nodes.push_back(node);
	if (parentNodeIndex >= 0)
	{
		chain.nodes[parentNodeIndex].childNodeIndices.push_back(nodeIndex);
	}

	const uint32 numBones = static_cast<uint32>(model->Get_NumBones());
	for (uint32 childIndex = 0; childIndex < numBones; ++childIndex)
	{
		if (model->Get_BoneParentIndex(childIndex) == boneIndex)
		{
			BuildChainRecursive(model, static_cast<int32>(childIndex), nodeIndex, chain);
		}
	}
}

void PhysicsModifier::Capture_CurrentPose(ModelCom* model, const Matrix& ownerWorld)
{
	if (!model)
	{
		return;
	}

	for (auto& chain : _chains)
	{
		if (chain.anchorBoneIndex >= 0)
		{
			if (const Matrix* anchorCombined = model->Get_BoneMatrixPtr(static_cast<uint32>(chain.anchorBoneIndex)))
			{
				chain.anchorWorld = (*anchorCombined) * ownerWorld;
			}
		}

		for (auto& node : chain.nodes)
		{
			node.poseLocal = model->Get_BoneLocalMatrix(node.boneIndex);
			if (!node.hasSmoothedLocalTransform)
			{
				Vec3 localScale = Vec3::One;
				Quat localRotation = Quat::Identity;
				Vec3 localTranslation = Vec3::Zero;
				Matrix local = node.poseLocal;
				local.Decompose(localScale, localRotation, localTranslation);
				node.smoothedLocalRotation = localRotation;
				node.smoothedLocalTranslation = localTranslation;
				node.hasSmoothedLocalTransform = true;
			}

			const Matrix* combined = model->Get_BoneMatrixPtr(node.boneIndex);
			if (!combined)
			{
				continue;
			}

			node.originalWorld = (*combined) * ownerWorld;
			node.position = Get_Position(node.originalWorld);

			if (node.parentNodeIndex >= 0)
			{
				const Node& parent = chain.nodes[node.parentNodeIndex];
				const Vec3 posePosition = Get_Position(node.originalWorld);
				node.length = Vec3::Distance(posePosition, Get_Position(parent.originalWorld));
				const Vec3 poseWorldDir = posePosition - Get_Position(parent.originalWorld);
				if (poseWorldDir.LengthSquared() > 0.0001f)
				{
					node.poseLocalDir = Get_LocalDirection(parent.originalWorld, poseWorldDir);
					if (!node.hasSmoothedLocalDir || node.smoothedLocalDir.LengthSquared() <= 0.0001f)
					{
						node.smoothedLocalDir = node.poseLocalDir;
						node.hasSmoothedLocalDir = node.poseLocalDir.LengthSquared() > 0.0001f;
					}
				}
			}
			else if (chain.anchorBoneIndex >= 0)
			{
				const Vec3 posePosition = Get_Position(node.originalWorld);
				const Vec3 anchorPosition = Get_Position(chain.anchorWorld);
				node.length = Vec3::Distance(posePosition, anchorPosition);
				const Vec3 poseWorldDir = posePosition - anchorPosition;
				if (poseWorldDir.LengthSquared() > 0.0001f)
				{
					node.poseLocalDir = Get_LocalDirection(chain.anchorWorld, poseWorldDir);
					if (!node.hasSmoothedLocalDir || node.smoothedLocalDir.LengthSquared() <= 0.0001f)
					{
						node.smoothedLocalDir = node.poseLocalDir;
						node.hasSmoothedLocalDir = node.poseLocalDir.LengthSquared() > 0.0001f;
					}
				}
			}
			else
			{
				node.length = 0.f;
				node.poseLocalDir = Vec3::Zero;
				node.smoothedLocalDir = Vec3::Zero;
				node.hasSmoothedLocalDir = false;
			}
		}
	}
}

void PhysicsModifier::Prepare_SolvedPoseStart()
{
	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			const Vec3 posePosition = Get_Position(node.originalWorld);
			if (!node.hasSolvedState)
			{
				node.solvedPosition = posePosition;
				node.prevSolvedPosition = posePosition;
				node.hasSolvedState = true;
			}

			const bool hadContactMemory =
				node.hasCollisionContact
				|| node.hasCollisionPushApplied
				|| node.collisionCorrectionDistance > 0.0001f
				|| node.contactCorrection.LengthSquared() > 0.00000001f
				|| node.smoothedContactCorrection.LengthSquared() > 0.00000001f
				|| node.wasCollisionDriven;
			const bool releaseMemory = node.contactReleaseTimer < std::max(0.f, _desc.releaseMemoryTime);
			const float followAlpha = hadContactMemory
				? std::clamp(_desc.contactPoseFollowAlpha, 0.f, 1.f)
				: (releaseMemory
					? std::clamp(_desc.releasePoseFollowAlpha, 0.f, 1.f)
					: std::clamp(_desc.freePoseFollowAlpha, 0.f, 1.f));
			const Vec3 baseTarget = hadContactMemory
				? node.solvedPosition
				: posePosition + node.inertiaOffsetWorld;
			node.position = Vec3::Lerp(node.solvedPosition, baseTarget, followAlpha);
		}
	}
}


void PhysicsModifier::Evaluate_SegmentCollisionContacts(const ModelCom* model, const Matrix& ownerWorld)
{
	_desc.debugCollisionHitCount = 0;
	_desc.debugSegmentCollisionHitCount = 0;
	_desc.debugMinCollisionMargin = 0.f;
	_desc.debugCollisionPushOutCount = 0;
	_desc.debugMaxCollisionPushOut = 0.f;

	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			node.hasCollisionContact = false;
			node.hasCollisionPushApplied = false;
			node.contactCorrection = Vec3::Zero;
			node.collisionCorrectionDistance = 0.f;
			node.smoothedContactCorrection = Vec3::Lerp(
				node.smoothedContactCorrection,
				Vec3::Zero,
				std::clamp(_desc.contactCorrectionReleaseAlpha, 0.f, 1.f));
		}
	}

	if (!_desc.enableCollision)
	{
		return;
	}

	const vector<WorldCapsule> worldCapsules = Update_WorldCapsules(model, ownerWorld);
	if (worldCapsules.empty())
	{
		return;
	}

	float minMargin = FLT_MAX;
	bool checkedAnyPair = false;

	for (auto& chain : _chains)
	{
		const float contactSkinWidth = std::max(0.f, _desc.collisionContactSkinWidth);
		for (size_t nodeIndex = 0; nodeIndex < chain.nodes.size(); ++nodeIndex)
		{
			Node& node = chain.nodes[nodeIndex];
			Vec3 segmentStart = Vec3::Zero;
			Vec3 segmentEnd = Vec3::Zero;
			if (node.parentNodeIndex >= 0)
			{
				segmentStart = chain.nodes[static_cast<size_t>(node.parentNodeIndex)].position;
				segmentEnd = node.position;
			}
			else if (chain.anchorBoneIndex >= 0)
			{
				segmentStart = Get_Position(chain.anchorWorld);
				segmentEnd = node.position;
			}
			else
			{
				continue;
			}

			bool nodeHit = false;
			float deepestPenetration = 0.f;
			Vec3 bestCorrection = Vec3::Zero;
			for (const WorldCapsule& capsule : worldCapsules)
			{
				const float radiusSum = GetCollisionQueryRadius(capsule);
				Vec3 segmentClosest = Vec3::Zero;
				Vec3 capsuleClosest = Vec3::Zero;
				const float distance = sqrtf(std::max(
					0.f,
					SegmentSegmentClosestPoints(segmentStart, segmentEnd, capsule.startPoint, capsule.endPoint, segmentClosest, capsuleClosest)));
				const float margin = distance - radiusSum;
				minMargin = std::min(minMargin, margin);
				checkedAnyPair = true;

				if (margin <= contactSkinWidth)
				{
					nodeHit = true;
					const float penetration = contactSkinWidth - margin;
					if (penetration > deepestPenetration)
					{
						deepestPenetration = penetration;
						bestCorrection = BuildEndpointCollisionCorrection(
							segmentStart,
							segmentEnd,
							segmentClosest,
							capsuleClosest,
							capsule.startPoint,
							capsule.endPoint,
							penetration,
							_resolvedCollisionPushDirectionWorld,
							_desc.collisionPushDirectionBlend);
					}
					++_desc.debugCollisionHitCount;
				}
			}

			if (nodeHit)
			{
				node.hasCollisionContact = true;
				node.collisionCorrectionDistance = deepestPenetration;
				++_desc.debugSegmentCollisionHitCount;

					if (bestCorrection.LengthSquared() > 0.00000001f)
					{
						const float correctionScale = std::max(0.f, _desc.collisionPushOutScale);
						const float maxCorrection = std::max(0.001f, node.length * std::max(0.f, _desc.collisionPushOutMaxScale));
						Vec3 correction = bestCorrection;
						correction *= correctionScale;
						const float depthDamping = 1.f + (std::clamp(_desc.tipCollisionPushDamping, 0.f, 1.f) - 1.f) * std::clamp(node.depthRatio, 0.f, 1.f);
						correction *= depthDamping;
						if (node.parentNodeIndex >= 0
							&& chain.nodes[static_cast<size_t>(node.parentNodeIndex)].hasCollisionPushApplied)
						{
							correction *= std::clamp(_desc.childCollisionPushDamping, 0.f, 1.f);
						}
						const float correctionLength = correction.Length();
						if (correctionLength > maxCorrection)
						{
							correction *= maxCorrection / correctionLength;
						}

						Vec3 correctionNormal = correction;
						if (correctionNormal.LengthSquared() > 0.0001f)
						{
							correctionNormal.Normalize();
							node.smoothedCollisionNormal = node.hasSmoothedCollisionNormal
								? Vec3::Lerp(
									node.smoothedCollisionNormal,
									correctionNormal,
									std::clamp(_desc.contactCorrectionSmoothAlpha, 0.f, 1.f))
								: correctionNormal;
							if (node.smoothedCollisionNormal.LengthSquared() > 0.0001f)
							{
								node.smoothedCollisionNormal.Normalize();
							}
							node.hasSmoothedCollisionNormal = true;
						}

						node.smoothedContactCorrection = Vec3::Lerp(
							node.smoothedContactCorrection,
							correction,
							std::clamp(_desc.contactCorrectionSmoothAlpha, 0.f, 1.f));

						node.contactCorrection = node.smoothedContactCorrection;
						node.hasCollisionPushApplied = true;
						++_desc.debugCollisionPushOutCount;
						_desc.debugMaxCollisionPushOut = std::max(_desc.debugMaxCollisionPushOut, node.contactCorrection.Length());
					}
				}
			}
	}

	if (checkedAnyPair)
	{
		_desc.debugMinCollisionMargin = minMargin;
	}
}

void PhysicsModifier::Solve_ContactPoseChains(
	const ModelCom* model,
	const Matrix& ownerWorld)
{
	const vector<WorldCapsule> worldCapsules = _desc.enableCollision
		? Update_WorldCapsules(model, ownerWorld)
		: vector<WorldCapsule>{};

	for (auto& chain : _chains)
	{
		for (size_t nodeIndex = 0; nodeIndex < chain.nodes.size(); ++nodeIndex)
		{
			const Vec3 correction = chain.nodes[nodeIndex].contactCorrection;
			if (correction.LengthSquared() <= 0.00000001f)
			{
				continue;
			}

			Node& contactNode = chain.nodes[nodeIndex];
			contactNode.position += correction;
			contactNode.hasCollisionPushApplied = true;
			contactNode.collisionCorrectionDistance = std::max(
				contactNode.collisionCorrectionDistance,
				chain.nodes[nodeIndex].collisionCorrectionDistance);
			Apply_CorrectionToDescendants(chain, nodeIndex, correction);
		}

		const uint32 iterationCount = std::max<uint32>(1, _desc.solverIterations);
		for (uint32 iteration = 0; iteration < iterationCount; ++iteration)
		{
			for (auto& node : chain.nodes)
			{
				Vec3 parentPosition = Vec3::Zero;
				if (node.parentNodeIndex >= 0)
				{
					parentPosition = chain.nodes[static_cast<size_t>(node.parentNodeIndex)].position;
				}
				else if (chain.anchorBoneIndex >= 0)
				{
					parentPosition = Get_Position(chain.anchorWorld);
				}
				else
				{
					node.position = Get_Position(node.originalWorld);
					continue;
				}

				const float targetLength = std::max(0.0001f, node.length);
				Vec3 direction = node.position - parentPosition;
				if (direction.LengthSquared() <= 0.000001f)
				{
					const Vec3 poseParentPosition = node.parentNodeIndex >= 0
						? Get_Position(chain.nodes[static_cast<size_t>(node.parentNodeIndex)].originalWorld)
						: Get_Position(chain.anchorWorld);
					direction = Get_Position(node.originalWorld) - poseParentPosition;
				}
				if (direction.LengthSquared() <= 0.000001f)
				{
					direction = Vec3::UnitY;
				}
				direction.Normalize();
				const bool collisionDriven =
					node.hasCollisionContact
					|| node.hasCollisionPushApplied
					|| node.collisionCorrectionDistance > 0.0001f;
				if (!collisionDriven && node.poseLocalDir.LengthSquared() > 0.0001f && _desc.contactSolveLimitDegree > 0.f)
				{
					const Matrix parentWorld = node.parentNodeIndex >= 0
						? chain.nodes[static_cast<size_t>(node.parentNodeIndex)].originalWorld
						: chain.anchorWorld;
					Vec3 targetLocalDir = Get_LocalDirection(parentWorld, direction);
					targetLocalDir = Limit_LocalDirectionByAngle(
						node.poseLocalDir,
						targetLocalDir,
						std::clamp(_desc.contactSolveLimitDegree, 0.f, 180.f));
					Vec3 limitedWorldDirection = Vec3::TransformNormal(targetLocalDir, parentWorld);
					if (limitedWorldDirection.LengthSquared() > 0.0001f)
					{
						limitedWorldDirection.Normalize();
						direction = limitedWorldDirection;
					}
				}
				node.position = parentPosition + direction * targetLength;
			}

			if (!worldCapsules.empty() && iteration == 0 && _desc.finalCollisionProjectionScale <= 0.f)
			{
				const float contactSkinWidth = std::max(0.f, _desc.collisionContactSkinWidth);

				for (size_t nodeIndex = 0; nodeIndex < chain.nodes.size(); ++nodeIndex)
				{
					Node& node = chain.nodes[nodeIndex];
					Vec3 segmentStart = Vec3::Zero;
					Vec3 segmentEnd = node.position;
					if (node.parentNodeIndex >= 0)
					{
						segmentStart = chain.nodes[static_cast<size_t>(node.parentNodeIndex)].position;
					}
					else if (chain.anchorBoneIndex >= 0)
					{
						segmentStart = Get_Position(chain.anchorWorld);
					}
					else
					{
						continue;
					}

					float deepestPenetration = 0.f;
					Vec3 bestCorrection = Vec3::Zero;
					for (const WorldCapsule& capsule : worldCapsules)
					{
						const float radiusSum = GetCollisionQueryRadius(capsule);
						Vec3 segmentClosest = Vec3::Zero;
						Vec3 capsuleClosest = Vec3::Zero;
						const float distance = sqrtf(std::max(
							0.f,
							SegmentSegmentClosestPoints(segmentStart, segmentEnd, capsule.startPoint, capsule.endPoint, segmentClosest, capsuleClosest)));
						const float margin = distance - radiusSum;
						if (margin > contactSkinWidth)
						{
							continue;
						}

						const float penetration = contactSkinWidth - margin;
						if (penetration <= deepestPenetration)
						{
							continue;
						}

						deepestPenetration = penetration;
						bestCorrection = BuildEndpointCollisionCorrection(
							segmentStart,
							segmentEnd,
							segmentClosest,
							capsuleClosest,
							capsule.startPoint,
							capsule.endPoint,
							penetration,
							_resolvedCollisionPushDirectionWorld,
							_desc.collisionPushDirectionBlend);
					}

					if (bestCorrection.LengthSquared() <= 0.00000001f)
					{
						continue;
					}

					const float correctionScale = std::max(0.f, _desc.collisionPushOutScale);
					const float maxCorrection = std::max(0.001f, node.length * std::max(0.f, _desc.collisionPushOutMaxScale));
					Vec3 correction = bestCorrection * correctionScale;
					const float correctionLength = correction.Length();
					if (correctionLength > maxCorrection)
					{
						correction *= maxCorrection / correctionLength;
					}

					node.position += correction;
					node.hasCollisionPushApplied = true;
					node.collisionCorrectionDistance = std::max(node.collisionCorrectionDistance, deepestPenetration);

					_desc.debugMaxCollisionPushOut = std::max(_desc.debugMaxCollisionPushOut, correction.Length());
				}
			}
		}

	}
}

void PhysicsModifier::Finalize_CollisionProjection(const ModelCom* model, const Matrix& ownerWorld)
{
	if (!_desc.enableCollision || _desc.finalCollisionProjectionScale <= 0.f)
	{
		return;
	}

	const vector<WorldCapsule> worldCapsules = Update_WorldCapsules(model, ownerWorld);
	if (worldCapsules.empty())
	{
		return;
	}

	const int32 finalProjectionIterations = std::clamp(_desc.finalCollisionProjectionIterations, 1, 8);
	for (auto& chain : _chains)
	{
		for (int32 projectionIteration = 0; projectionIteration < finalProjectionIterations; ++projectionIteration)
		{
			Enforce_ChainLengthConstraints(chain);
			Project_ChainOutOfColliders(
				chain,
				worldCapsules,
				std::max(0.f, _desc.finalCollisionProjectionScale),
				std::max(0.f, _desc.finalCollisionProjectionMaxScale),
				true);
			HardProject_SegmentsOutOfColliders(chain, worldCapsules, true);
			HardProject_NodePointsOutOfColliders(chain, worldCapsules, true);
		}
	}
}

void PhysicsModifier::Commit_SolvedState()
{
	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			node.prevSolvedPosition = node.hasSolvedState ? node.solvedPosition : node.position;
			node.solvedPosition = node.position;
			node.hasSolvedState = true;
		}
	}
}

void PhysicsModifier::Enforce_ChainLengthConstraints(Chain& chain)
{
	for (auto& node : chain.nodes)
	{
		Vec3 parentPosition = Vec3::Zero;
		if (node.parentNodeIndex >= 0)
		{
			parentPosition = chain.nodes[static_cast<size_t>(node.parentNodeIndex)].position;
		}
		else if (chain.anchorBoneIndex >= 0)
		{
			parentPosition = Get_Position(chain.anchorWorld);
		}
		else
		{
			node.position = Get_Position(node.originalWorld);
			continue;
		}

		const float targetLength = std::max(0.0001f, node.length);
		Vec3 direction = node.position - parentPosition;
		if (direction.LengthSquared() <= 0.000001f)
		{
			const Vec3 poseParentPosition = node.parentNodeIndex >= 0
				? Get_Position(chain.nodes[static_cast<size_t>(node.parentNodeIndex)].originalWorld)
				: Get_Position(chain.anchorWorld);
			direction = Get_Position(node.originalWorld) - poseParentPosition;
		}
		if (direction.LengthSquared() <= 0.000001f)
		{
			direction = Vec3::UnitY;
		}
		direction.Normalize();

		const bool collisionDriven =
			node.hasCollisionContact
			|| node.hasCollisionPushApplied
			|| node.collisionCorrectionDistance > 0.0001f;
		if (!collisionDriven && node.poseLocalDir.LengthSquared() > 0.0001f && _desc.contactSolveLimitDegree > 0.f)
		{
			const Matrix parentWorld = node.parentNodeIndex >= 0
				? chain.nodes[static_cast<size_t>(node.parentNodeIndex)].originalWorld
				: chain.anchorWorld;
			Vec3 targetLocalDir = Get_LocalDirection(parentWorld, direction);
			targetLocalDir = Limit_LocalDirectionByAngle(
				node.poseLocalDir,
				targetLocalDir,
				std::clamp(_desc.contactSolveLimitDegree, 0.f, 180.f));
			Vec3 limitedWorldDirection = Vec3::TransformNormal(targetLocalDir, parentWorld);
			if (limitedWorldDirection.LengthSquared() > 0.0001f)
			{
				limitedWorldDirection.Normalize();
				direction = limitedWorldDirection;
			}
		}

		node.position = parentPosition + direction * targetLength;
	}
}

void PhysicsModifier::Update_ResolvedCollisionPushDirection(const Matrix& ownerWorld)
{
	_resolvedCollisionPushDirectionWorld = Vec3::Zero;
	if (_desc.collisionPushDirectionWorld.LengthSquared() <= 0.0001f)
	{
		return;
	}

	_resolvedCollisionPushDirectionWorld = Vec3::TransformNormal(_desc.collisionPushDirectionWorld, ownerWorld);
	if (_resolvedCollisionPushDirectionWorld.LengthSquared() > 0.0001f)
	{
		_resolvedCollisionPushDirectionWorld.Normalize();
	}
	else
	{
		_resolvedCollisionPushDirectionWorld = Vec3::Zero;
	}
}

void PhysicsModifier::Project_ChainOutOfColliders(
	Chain& chain,
	const vector<WorldCapsule>& worldCapsules,
	float scale,
	float maxScale,
	bool markContact)
{
	if (worldCapsules.empty())
	{
		return;
	}

	const float contactSkinWidth = std::max(0.f, _desc.collisionContactSkinWidth);

	for (size_t nodeIndex = 0; nodeIndex < chain.nodes.size(); ++nodeIndex)
	{
		Node& node = chain.nodes[nodeIndex];
		Vec3 segmentStart = Vec3::Zero;
		Vec3 segmentEnd = node.position;
		if (node.parentNodeIndex >= 0)
		{
			segmentStart = chain.nodes[static_cast<size_t>(node.parentNodeIndex)].position;
		}
		else if (chain.anchorBoneIndex >= 0)
		{
			segmentStart = Get_Position(chain.anchorWorld);
		}
		else
		{
			continue;
		}

		float deepestPenetration = 0.f;
		Vec3 bestCorrection = Vec3::Zero;
		Vec3 bestSegmentClosest = Vec3::Zero;
		Vec3 bestCapsuleClosest = Vec3::Zero;
		Vec3 bestCapsuleStart = Vec3::Zero;
		Vec3 bestCapsuleEnd = Vec3::Zero;
		float bestRadiusSum = 0.f;
		for (const WorldCapsule& capsule : worldCapsules)
		{
			const float radiusSum = GetCollisionQueryRadius(capsule);
			Vec3 segmentClosest = Vec3::Zero;
			Vec3 capsuleClosest = Vec3::Zero;
			const float distance = sqrtf(std::max(
				0.f,
				SegmentSegmentClosestPoints(segmentStart, segmentEnd, capsule.startPoint, capsule.endPoint, segmentClosest, capsuleClosest)));
			const float margin = distance - radiusSum;
			if (margin > contactSkinWidth)
			{
				continue;
			}

			const float penetration = contactSkinWidth - margin;
			if (penetration <= deepestPenetration)
			{
				continue;
			}

			deepestPenetration = penetration;
			bestSegmentClosest = segmentClosest;
			bestCapsuleClosest = capsuleClosest;
			bestCapsuleStart = capsule.startPoint;
			bestCapsuleEnd = capsule.endPoint;
			bestRadiusSum = radiusSum;
			bestCorrection = BuildEndpointCollisionCorrection(
				segmentStart,
				segmentEnd,
				segmentClosest,
				capsuleClosest,
				capsule.startPoint,
				capsule.endPoint,
				penetration,
				_resolvedCollisionPushDirectionWorld,
				_desc.collisionPushDirectionBlend);
		}

		if (bestCorrection.LengthSquared() <= 0.00000001f)
		{
			continue;
		}

		Vec3 blockedPosition = Vec3::Zero;
		const float targetLength = std::max(0.0001f, node.length);
		if (BuildCollisionDirectionBlockPosition(
			segmentStart,
			segmentEnd,
			bestSegmentClosest,
			bestCapsuleClosest,
			bestCapsuleStart,
			bestCapsuleEnd,
			targetLength,
			bestRadiusSum,
			contactSkinWidth,
			_desc.collisionDirectionBlockAlpha,
			_resolvedCollisionPushDirectionWorld,
			_desc.collisionPushDirectionBlend,
			blockedPosition))
		{
			node.position = blockedPosition;
			segmentEnd = node.position;
		}

		const float depthDamping = 1.f + (std::clamp(_desc.tipCollisionPushDamping, 0.f, 1.f) - 1.f) * std::clamp(node.depthRatio, 0.f, 1.f);
		float correctionScale = std::max(0.f, scale) * depthDamping;
		if (node.parentNodeIndex >= 0
			&& chain.nodes[static_cast<size_t>(node.parentNodeIndex)].hasCollisionPushApplied)
		{
			correctionScale *= std::clamp(_desc.childCollisionPushDamping, 0.f, 1.f);
		}

		const float maxCorrection = std::max(0.001f, node.length * std::max(0.f, maxScale));
		Vec3 parentCorrection = Vec3::Zero;
		Vec3 nodeCorrection = Vec3::Zero;
		const bool canMoveParent = node.parentNodeIndex >= 0;
		if (!BuildSegmentCollisionCorrections(
			segmentStart,
			segmentEnd,
			bestSegmentClosest,
			bestCapsuleClosest,
			bestCapsuleStart,
			bestCapsuleEnd,
			deepestPenetration,
			correctionScale,
			maxCorrection,
			canMoveParent,
			_resolvedCollisionPushDirectionWorld,
			_desc.collisionPushDirectionBlend,
			parentCorrection,
			nodeCorrection))
		{
			continue;
		}

		if (canMoveParent && parentCorrection.LengthSquared() > 0.00000001f)
		{
			const size_t parentIndex = static_cast<size_t>(node.parentNodeIndex);
			Apply_NodeCorrection(chain, parentIndex, parentCorrection, deepestPenetration, true);
		}

		Apply_NodeCorrection(chain, nodeIndex, nodeCorrection, deepestPenetration, true);
		if (markContact)
		{
			node.hasCollisionContact = true;
		}
		node.hasCollisionPushApplied = true;
		node.collisionCorrectionDistance = std::max(node.collisionCorrectionDistance, deepestPenetration);
	}
}

void PhysicsModifier::HardProject_SegmentsOutOfColliders(
	Chain& chain,
	const vector<WorldCapsule>& worldCapsules,
	bool markContact)
{
	if (worldCapsules.empty())
	{
		return;
	}

	for (size_t nodeIndex = 0; nodeIndex < chain.nodes.size(); ++nodeIndex)
	{
		Node& node = chain.nodes[nodeIndex];
		Vec3 segmentStart = Vec3::Zero;
		Vec3 segmentEnd = node.position;
		const bool canMoveParent = node.parentNodeIndex >= 0;
		if (canMoveParent)
		{
			segmentStart = chain.nodes[static_cast<size_t>(node.parentNodeIndex)].position;
		}
		else if (chain.anchorBoneIndex >= 0)
		{
			segmentStart = Get_Position(chain.anchorWorld);
		}
		else
		{
			continue;
		}

		float deepestPenetration = 0.f;
		Vec3 parentCorrection = Vec3::Zero;
		Vec3 nodeCorrection = Vec3::Zero;
		for (const WorldCapsule& capsule : worldCapsules)
		{
			const float minDistance = GetCollisionContactDistance(capsule);
			Vec3 segmentClosest = Vec3::Zero;
			Vec3 capsuleClosest = Vec3::Zero;
			const float distance = sqrtf(std::max(
				0.f,
				SegmentSegmentClosestPoints(segmentStart, segmentEnd, capsule.startPoint, capsule.endPoint, segmentClosest, capsuleClosest)));
			const float penetration = minDistance - distance;
			if (penetration <= 0.f || penetration <= deepestPenetration)
			{
				continue;
			}

			Vec3 startCorrection = Vec3::Zero;
			Vec3 endCorrection = Vec3::Zero;
			if (!BuildHardSegmentCollisionCorrections(
				segmentStart,
				segmentEnd,
				segmentClosest,
				capsuleClosest,
				capsule.startPoint,
				capsule.endPoint,
				penetration,
				canMoveParent,
				_resolvedCollisionPushDirectionWorld,
				_desc.collisionPushDirectionBlend,
				startCorrection,
				endCorrection))
			{
				continue;
			}

			deepestPenetration = penetration;
			parentCorrection = startCorrection;
			nodeCorrection = endCorrection;
		}

		if (deepestPenetration <= 0.f)
		{
			continue;
		}

		const float maxHardCorrection = std::max(
			0.001f,
			std::max(0.0001f, node.length) * std::max(0.01f, _desc.finalCollisionProjectionMaxScale));
		const float snapAlpha = std::clamp(_desc.contactSnapAlpha, 0.f, 1.f);
		parentCorrection = Clamp_VectorLength(parentCorrection, maxHardCorrection) * snapAlpha;
		nodeCorrection = Clamp_VectorLength(nodeCorrection, maxHardCorrection) * snapAlpha;

		if (canMoveParent && parentCorrection.LengthSquared() > 0.00000001f)
		{
			const size_t parentIndex = static_cast<size_t>(node.parentNodeIndex);
			Apply_NodeCorrection(chain, parentIndex, parentCorrection, deepestPenetration, true);
		}

		Apply_NodeCorrection(chain, nodeIndex, nodeCorrection, deepestPenetration, true);

		if (markContact)
		{
			node.hasCollisionContact = true;
		}
		node.hasCollisionPushApplied = true;
		node.collisionCorrectionDistance = std::max(node.collisionCorrectionDistance, deepestPenetration);
	}
}

void PhysicsModifier::HardProject_NodePointsOutOfColliders(
	Chain& chain,
	const vector<WorldCapsule>& worldCapsules,
	bool markContact)
{
	if (worldCapsules.empty())
	{
		return;
	}

	for (size_t nodeIndex = 0; nodeIndex < chain.nodes.size(); ++nodeIndex)
	{
		Node& node = chain.nodes[nodeIndex];
		Vec3 projectedPosition = node.position;
		float deepestPenetration = 0.f;
		bool projected = false;

		for (const WorldCapsule& capsule : worldCapsules)
		{
			const float minDistance = GetCollisionContactDistance(capsule);
			const Vec3 closest = ClosestPointOnSegment(projectedPosition, capsule.startPoint, capsule.endPoint);
			Vec3 normal = projectedPosition - closest;
			float distance = normal.Length();
			if (distance >= minDistance)
			{
				continue;
			}

			if (normal.LengthSquared() <= 0.0001f)
			{
				normal = Get_Position(node.originalWorld) - closest;
			}
			if (normal.LengthSquared() <= 0.0001f)
			{
				normal = Vec3::Up;
			}
			normal.Normalize();

			const float penetration = minDistance - distance;
			projectedPosition = closest + normal * minDistance;
			deepestPenetration = std::max(deepestPenetration, penetration);
			projected = true;
		}

		if (!projected)
		{
			continue;
		}

		const float maxHardCorrection = std::max(
			0.001f,
			std::max(0.0001f, node.length) * std::max(0.01f, _desc.finalCollisionProjectionMaxScale));
		const Vec3 correction = Clamp_VectorLength(projectedPosition - node.position, maxHardCorrection)
			* std::clamp(_desc.contactSnapAlpha, 0.f, 1.f);
		Apply_NodeCorrection(chain, nodeIndex, correction, deepestPenetration, false);
		if (markContact)
		{
			node.hasCollisionContact = true;
		}
		node.hasCollisionPushApplied = true;
		node.collisionCorrectionDistance = std::max(node.collisionCorrectionDistance, deepestPenetration);
	}
}

void PhysicsModifier::Update_InertiaOffsets(
	const Matrix& ownerWorld,
	const Matrix& prevOwnerWorld,
	const Vec3& ownerDelta)
{
	_desc.debugInertiaMagnitude = 0.f;

	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			const Vec3 posePosition = Get_Position(node.originalWorld);
			Vec3 poseDir = node.poseLocalDir;
			if (node.parentNodeIndex >= 0)
			{
				const Vec3 parentPosePosition = Get_Position(chain.nodes[static_cast<size_t>(node.parentNodeIndex)].originalWorld);
				poseDir = posePosition - parentPosePosition;
			}
			else if (chain.anchorBoneIndex >= 0)
			{
				poseDir = posePosition - Get_Position(chain.anchorWorld);
			}

			if (poseDir.LengthSquared() > 0.0001f)
			{
				poseDir.Normalize();
			}

			if (!node.hasPrevPoseState)
			{
				node.prevPosePosition = posePosition;
				node.prevPoseDir = poseDir;
				node.hasPrevPoseState = true;
			}

			const Vec3 animPosDelta = posePosition - node.prevPosePosition;
			Vec3 animRotDelta = Vec3::Zero;
			if (node.prevPoseDir.LengthSquared() > 0.0001f && poseDir.LengthSquared() > 0.0001f)
			{
				animRotDelta = (poseDir - node.prevPoseDir) * std::max(0.0001f, node.length);
			}

			Vec3 ownerRotDelta = Vec3::Zero;
			const Matrix prevOwnerInv = prevOwnerWorld.Invert();
			const Vec3 ownerLocalPose = Vec3::Transform(posePosition, prevOwnerInv);
			const Vec3 ownerRotatedPose = Vec3::Transform(ownerLocalPose, ownerWorld);
			ownerRotDelta = ownerRotatedPose - posePosition;

			Vec3 rawInertia = Vec3::Zero;
			rawInertia -= animPosDelta * std::max(0.f, _desc.animTranslationInertiaScale);
			rawInertia -= animRotDelta * std::max(0.f, _desc.animRotationInertiaScale);
			rawInertia -= ownerDelta * std::max(0.f, _desc.ownerTranslationInertiaScale);
			rawInertia -= ownerRotDelta * std::max(0.f, _desc.ownerRotationInertiaScale);
			rawInertia = Clamp_VectorLength(rawInertia, std::max(0.f, _desc.inertiaMaxDistance));

			const bool contactMemory =
				node.wasCollisionDriven
				|| node.hasCollisionContact
				|| node.hasCollisionPushApplied
				|| node.collisionCorrectionDistance > 0.0001f;
			const float damping = contactMemory
				? std::clamp(_desc.contactInertiaDamping, 0.f, 1.f)
				: std::clamp(_desc.inertiaDamping, 0.f, 1.f);
			node.inertiaOffsetWorld = Vec3::Lerp(node.inertiaOffsetWorld, rawInertia, 1.f - damping);
			node.inertiaOffsetWorld = Clamp_VectorLength(node.inertiaOffsetWorld, std::max(0.f, _desc.inertiaMaxDistance));

			_desc.debugInertiaMagnitude = std::max(_desc.debugInertiaMagnitude, node.inertiaOffsetWorld.Length());
			node.prevPosePosition = posePosition;
			node.prevPoseDir = poseDir;
		}
	}
}

void PhysicsModifier::Update_ContactReleaseStates(float timeDelta)
{
	_desc.debugContactNodeCount = 0;
	_desc.debugReleaseNodeCount = 0;

	const float releaseTime = std::max(0.f, _desc.releaseMemoryTime);
	const float releaseCorrectionAlpha = std::clamp(_desc.releaseContactCorrectionAlpha, 0.f, 1.f);
	const float releaseInertiaAlpha = std::clamp(_desc.releaseInertiaDamping, 0.f, 1.f);

	for (auto& chain : _chains)
	{
		for (auto& node : chain.nodes)
		{
			const bool collisionDriven =
				node.hasCollisionContact
				|| node.hasCollisionPushApplied
				|| node.collisionCorrectionDistance > 0.0001f;
			if (collisionDriven)
			{
				node.contactReleaseTimer = 0.f;
				node.wasCollisionDriven = true;
				++_desc.debugContactNodeCount;
				continue;
			}

			if (node.wasCollisionDriven)
			{
				node.contactReleaseTimer += std::max(0.f, timeDelta);
				if (node.contactReleaseTimer < releaseTime)
				{
					node.smoothedContactCorrection = Vec3::Lerp(node.smoothedContactCorrection, Vec3::Zero, releaseCorrectionAlpha);
					node.inertiaOffsetWorld = Vec3::Lerp(node.inertiaOffsetWorld, Vec3::Zero, releaseInertiaAlpha);
					++_desc.debugReleaseNodeCount;
				}
				else
				{
					node.wasCollisionDriven = false;
					node.contactReleaseTimer = releaseTime;
					node.smoothedContactCorrection = Vec3::Zero;
				}
			}
		}
	}
}

void PhysicsModifier::Apply_NodeCorrection(
	Chain& chain,
	size_t nodeIndex,
	const Vec3& correction,
	float penetration,
	bool propagateToChildren)
{
	if (nodeIndex >= chain.nodes.size() || correction.LengthSquared() <= 0.00000001f)
	{
		return;
	}

	Node& node = chain.nodes[nodeIndex];
	Vec3 appliedCorrection = correction;
	const float correctionLength = appliedCorrection.Length();
	if (correctionLength > 0.000001f)
	{
		Vec3 correctionDir = appliedCorrection / correctionLength;
		if (node.hasSmoothedCollisionNormal && node.smoothedCollisionNormal.LengthSquared() > 0.0001f)
		{
			correctionDir = Vec3::Lerp(
				node.smoothedCollisionNormal,
				correctionDir,
				std::clamp(_desc.collisionDirectionSmoothAlpha, 0.f, 1.f));
			if (correctionDir.LengthSquared() <= 0.0001f)
			{
				correctionDir = appliedCorrection / correctionLength;
			}
		}
		if (correctionDir.LengthSquared() > 0.0001f)
		{
			correctionDir.Normalize();
			node.smoothedCollisionNormal = correctionDir;
			node.hasSmoothedCollisionNormal = true;
			appliedCorrection = correctionDir * correctionLength;
		}
	}

	node.position += appliedCorrection;
	node.hasCollisionPushApplied = true;
	node.collisionCorrectionDistance = std::max(node.collisionCorrectionDistance, penetration);
	_desc.debugMaxCollisionPushOut = std::max(_desc.debugMaxCollisionPushOut, appliedCorrection.Length());

	if (propagateToChildren)
	{
		Apply_CorrectionToDescendants(chain, nodeIndex, appliedCorrection);
	}
}

void PhysicsModifier::Apply_CorrectionToDescendants(Chain& chain, size_t nodeIndex, const Vec3& correction)
{
	const float propagationAlpha = std::clamp(_desc.contactBendPropagationAlpha, 0.f, 1.f);
	if (propagationAlpha <= 0.0001f || correction.LengthSquared() <= 0.00000001f)
	{
		return;
	}

	const float propagationDecay = std::clamp(_desc.contactBendPropagationDecay, 0.f, 1.f);
	vector<pair<int32, float>> pending{};
	for (int32 childIndex : chain.nodes[nodeIndex].childNodeIndices)
	{
		pending.emplace_back(childIndex, propagationAlpha);
	}

	while (!pending.empty())
	{
		const auto [childIndex, weight] = pending.back();
		pending.pop_back();
		if (childIndex < 0 || static_cast<size_t>(childIndex) >= chain.nodes.size() || weight <= 0.0001f)
		{
			continue;
		}

		Node& childNode = chain.nodes[static_cast<size_t>(childIndex)];
		const Vec3 propagatedCorrection = correction * weight;
		childNode.position += propagatedCorrection;
		childNode.hasCollisionPushApplied = true;
		childNode.collisionCorrectionDistance = std::max(
			childNode.collisionCorrectionDistance,
			chain.nodes[nodeIndex].collisionCorrectionDistance * weight);
		_desc.debugMaxCollisionPushOut = std::max(_desc.debugMaxCollisionPushOut, propagatedCorrection.Length());

		const float childWeight = weight * propagationDecay;
		for (int32 nextChildIndex : childNode.childNodeIndices)
		{
			pending.emplace_back(nextChildIndex, childWeight);
		}
	}
}


void PhysicsModifier::Write_BackSolvedPoseToModel(ModelCom* model, const Matrix& ownerWorld, bool isOwnerIdle)
{
	if (!model || !_desc.writeBackEnabled)
	{
		return;
	}

	const float baseOutputAlpha = std::clamp(_strengthAlpha * _desc.outputAlpha, 0.f, 1.f);
	if (baseOutputAlpha <= 0.0001f)
	{
		return;
	}

	const vector<WorldCapsule> writeBackCapsules = _desc.enableCollision
		? Update_WorldCapsules(model, ownerWorld)
		: vector<WorldCapsule>{};
	if (!writeBackCapsules.empty())
	{
		const int32 finalProjectionIterations = std::clamp(_desc.finalCollisionProjectionIterations, 1, 8);
		for (auto& chain : _chains)
		{
			for (int32 projectionIteration = 0; projectionIteration < finalProjectionIterations; ++projectionIteration)
			{
				HardProject_SegmentsOutOfColliders(chain, writeBackCapsules, true);
				HardProject_NodePointsOutOfColliders(chain, writeBackCapsules, true);
			}
		}
	}

	for (auto& chain : _chains)
	{
		const float chainOutputAlpha = std::clamp(baseOutputAlpha * std::max(0.f, chain.desc.outputMultiplier), 0.f, 1.f);
		if (chainOutputAlpha <= 0.0001f)
		{
			continue;
		}

		for (auto& node : chain.nodes)
		{
			model->Set_BoneLocalMatrix(node.boneIndex, node.poseLocal);
		}
		model->Rebuild_BoneCombinedMatrices();

		const float smoothAlpha = std::clamp(
			(isOwnerIdle ? _desc.idleOutputSmoothAlpha : _desc.outputSmoothAlpha) * std::max(0.f, chain.desc.smoothMultiplier),
			0.f,
			1.f);

		for (auto& node : chain.nodes)
		{
			if (node.childNodeIndices.empty())
			{
				continue;
			}

			int32 childNodeIndex = node.childNodeIndices.front();
			for (int32 candidateIndex : node.childNodeIndices)
			{
				const Node& candidate = chain.nodes[static_cast<size_t>(candidateIndex)];
				const Node& selected = chain.nodes[static_cast<size_t>(childNodeIndex)];
				if (candidate.hasCollisionContact && !selected.hasCollisionContact)
				{
					childNodeIndex = candidateIndex;
					continue;
				}

				if (candidate.collisionCorrectionDistance > selected.collisionCorrectionDistance)
				{
					childNodeIndex = candidateIndex;
				}
			}

			const Node& childNode = chain.nodes[static_cast<size_t>(childNodeIndex)];
			if (childNode.poseLocalDir.LengthSquared() <= 0.0001f)
			{
				continue;
			}

			const Matrix* nodeCombined = model->Get_BoneMatrixPtr(node.boneIndex);
			if (!nodeCombined)
			{
				continue;
			}

			Vec3 poseScale = Vec3::One;
			Quat poseRotation = Quat::Identity;
			Vec3 poseTranslation = Vec3::Zero;
			Matrix poseLocal = node.poseLocal;
			poseLocal.Decompose(poseScale, poseRotation, poseTranslation);

			const Matrix nodeWorld = (*nodeCombined) * ownerWorld;
			Vec3 targetWorldDir = childNode.position - node.position;
			const float solvedSegmentLength = targetWorldDir.Length();
			if (solvedSegmentLength <= 0.0001f)
			{
				continue;
			}
			targetWorldDir.Normalize();

			Vec3 targetLocalDir = Get_LocalDirection(nodeWorld, targetWorldDir);
			if (targetLocalDir.LengthSquared() <= 0.0001f)
			{
				continue;
			}
			const bool hasContactOutput =
				(childNode.hasCollisionContact || childNode.hasCollisionPushApplied)
				&& childNode.collisionCorrectionDistance > 0.0001f;
			const bool hasNodePositionOutput =
				(node.hasCollisionContact || node.hasCollisionPushApplied)
				&& node.collisionCorrectionDistance > 0.0001f;
			const float outputLimitDegree = hasContactOutput
				? std::max(std::clamp(_desc.contactOutputLimitDegree, 0.f, 180.f), 150.f)
				: std::clamp(_desc.contactOutputLimitDegree, 0.f, 180.f);
			targetLocalDir = Limit_LocalDirectionByAngle(childNode.poseLocalDir, targetLocalDir, outputLimitDegree);

			Quat solvedRotation = Quat::Identity;
			Vec3 targetLocalTranslation = poseTranslation;
			bool hasSegmentFrameOutput = false;
			if (_desc.segmentFrameOutput)
			{
				Vec3 limitedWorldDir = Vec3::TransformNormal(targetLocalDir, nodeWorld);
				if (limitedWorldDir.LengthSquared() > 0.0001f)
				{
					limitedWorldDir.Normalize();
					const Vec3 limitedChildPosition = node.position + limitedWorldDir * solvedSegmentLength;

					Matrix poseSegmentWorld = Matrix::Identity;
					Matrix solvedSegmentWorld = Matrix::Identity;
					if (Build_SegmentWorldFrame(Get_Position(node.originalWorld), Get_Position(childNode.originalWorld), poseSegmentWorld)
						&& Build_SegmentWorldFrame(node.position, limitedChildPosition, solvedSegmentWorld))
					{
						Matrix parentWorld = ownerWorld;
						const int32 parentBoneIndex = model->Get_BoneParentIndex(node.boneIndex);
						if (parentBoneIndex >= 0)
						{
							if (const Matrix* parentCombined = model->Get_BoneMatrixPtr(static_cast<uint32>(parentBoneIndex)))
							{
								parentWorld = (*parentCombined) * ownerWorld;
							}
						}

						const Matrix segmentToBoneRest = poseSegmentWorld.Invert() * node.originalWorld;
						const Matrix targetBoneWorld = solvedSegmentWorld * segmentToBoneRest;
						Matrix targetLocal = targetBoneWorld * parentWorld.Invert();

						Vec3 matrixLocalScale = Vec3::One;
						Quat matrixLocalRotation = Quat::Identity;
						Vec3 matrixLocalTranslation = Vec3::Zero;
						targetLocal.Decompose(matrixLocalScale, matrixLocalRotation, matrixLocalTranslation);
						matrixLocalRotation.Normalize();

						solvedRotation = matrixLocalRotation;
						targetLocalTranslation = matrixLocalTranslation;
						hasSegmentFrameOutput = true;
					}
				}
			}

			if (!hasSegmentFrameOutput)
			{
				const Quat targetDelta = Build_RotationDelta(childNode.poseLocalDir, targetLocalDir);
				solvedRotation = targetDelta * poseRotation;
				solvedRotation.Normalize();
			}

			if (hasNodePositionOutput && _desc.translationOutputAlpha > 0.f)
			{
				Matrix parentWorld = ownerWorld;
				const int32 parentBoneIndex = model->Get_BoneParentIndex(node.boneIndex);
				if (parentBoneIndex >= 0)
				{
					if (const Matrix* parentCombined = model->Get_BoneMatrixPtr(static_cast<uint32>(parentBoneIndex)))
					{
						parentWorld = (*parentCombined) * ownerWorld;
					}
				}

				targetLocalTranslation = Vec3::Transform(node.position, parentWorld.Invert());
			}

			if (!node.hasSmoothedLocalTransform)
			{
				node.smoothedLocalTranslation = poseTranslation;
				node.smoothedLocalRotation = poseRotation;
				node.hasSmoothedLocalTransform = true;
			}

			const float depthWeight = SmoothStep(node.depthRatio);
			const float tipSmoothScale = std::clamp(_desc.tipOutputSmoothScale, 0.f, 1.f);
			const float depthSmoothScale = 1.f + (tipSmoothScale - 1.f) * depthWeight;
			const float nodeSmoothAlpha = hasContactOutput
				? std::max(std::clamp(smoothAlpha * depthSmoothScale, 0.f, 1.f), std::clamp(_desc.contactOutputSmoothAlpha, 0.f, 1.f))
				: std::clamp(smoothAlpha * depthSmoothScale, 0.f, 1.f);

			const float nodeOutputAlpha = hasContactOutput
				? std::max(chainOutputAlpha, std::clamp(_desc.contactOutputAlpha * std::max(0.f, chain.desc.outputMultiplier), 0.f, 1.f))
				: chainOutputAlpha;
			const float baseRotationOutputAlpha = std::clamp(
				nodeOutputAlpha * std::clamp(_desc.rotationOutputAlpha, 0.f, 1.f),
				0.f,
				1.f);
			const float contactDepthReference = std::max(0.0001f, node.length * 0.35f);
			const float contactDepthAlpha = SmoothStep(std::clamp(
				std::max(childNode.collisionCorrectionDistance, node.collisionCorrectionDistance) / contactDepthReference,
				0.f,
				1.f));
			const float contactRotationOutputAlpha = std::clamp(
				_desc.contactOutputAlpha * std::max(0.f, chain.desc.outputMultiplier) * 0.45f * contactDepthAlpha,
				0.f,
				1.f);
			const float nodeRotationOutputAlpha = hasContactOutput
				? std::max(baseRotationOutputAlpha, contactRotationOutputAlpha)
				: baseRotationOutputAlpha;
			Quat targetRotation = Quat::Slerp(poseRotation, solvedRotation, nodeRotationOutputAlpha);
			targetRotation.Normalize();

			node.smoothedLocalRotation = Quat::Slerp(node.smoothedLocalRotation, targetRotation, nodeSmoothAlpha);
			node.smoothedLocalRotation.Normalize();

			Vec3 targetTranslation = poseTranslation;
			if ((hasSegmentFrameOutput || hasNodePositionOutput) && _desc.translationOutputAlpha > 0.f)
			{
				const float maxTranslationDelta = std::max(0.f, node.length * std::max(0.f, _desc.translationOutputLimitScale));
				const Vec3 limitedTranslation = Clamp_LocalTranslationFromPose(
					poseTranslation,
					targetLocalTranslation,
					maxTranslationDelta);
				targetTranslation = Vec3::Lerp(
					poseTranslation,
					limitedTranslation,
					std::clamp(_desc.translationOutputAlpha, 0.f, 1.f));
			}
			node.smoothedLocalTranslation = hasNodePositionOutput
				? targetTranslation
				: Vec3::Lerp(node.smoothedLocalTranslation, targetTranslation, nodeSmoothAlpha);

			model->Set_BoneLocalMatrix(node.boneIndex, Build_LocalMatrix_FromSRT(
				poseScale,
				node.smoothedLocalRotation,
				node.smoothedLocalTranslation));
			model->Rebuild_BoneCombinedMatrices();
		}

	}
}

vector<PhysicsModifier::WorldCapsule> PhysicsModifier::Update_WorldCapsules(const ModelCom* model, const Matrix& ownerWorld) const
{
	vector<WorldCapsule> worldCapsules{};
	worldCapsules.reserve(_colliders.size());
	_desc.debugLoadedCapsuleCount = static_cast<int32>(_colliders.size());
	_desc.debugActiveCapsuleCount = 0;
	_desc.debugCollisionSource = _desc.collisionModel ? 1 : 0;

	const ModelCom* collisionModel = _desc.collisionModel ? _desc.collisionModel : model;
	const Matrix collisionOwnerWorld = _desc.collisionOwnerWorldMatrix ? *_desc.collisionOwnerWorldMatrix : ownerWorld;
	if (!collisionModel)
	{
		return worldCapsules;
	}

	for (const auto& col : _colliders)
	{
		if (!_desc.includedCollisionBones.empty()
			&& std::find(_desc.includedCollisionBones.begin(), _desc.includedCollisionBones.end(), col.boneName) == _desc.includedCollisionBones.end())
		{
			continue;
		}

		const int32 boneIndex = collisionModel->Get_BoneIndex(col.boneName);
		if (boneIndex < 0)
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

void PhysicsModifier::Evaluate_Modifier(ModelCom* model, float timeDelta)
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
	const Vec3 ownerPosition = Get_Position(ownerWorld);
	const Matrix prevOwnerWorld = _prevOwnerWorld;
	Update_ResolvedCollisionPushDirection(ownerWorld);
	if (_isFirstUpdate)
	{
		_prevOwnerPos = ownerPosition;
		_prevOwnerWorld = ownerWorld;
		ResetPose(model);
		return;
	}

	Vec3 ownerDelta = ownerPosition - _prevOwnerPos;
	if (_runtimeTestOwnerMotionEnabled)
	{
		ownerDelta += _runtimeTestOwnerMotionDelta;
	}
	if (ownerDelta.Length() >= _desc.teleportDistance)
	{
		ResetPose(model);
		ownerDelta = Vec3::Zero;
	}
	_prevOwnerPos = ownerPosition;
	_prevOwnerWorld = ownerWorld;
	const bool isOwnerIdle = ownerDelta.Length() <= _desc.idleMotionDeadzone;

	Capture_CurrentPose(model, ownerWorld);
	Update_InertiaOffsets(ownerWorld, prevOwnerWorld, ownerDelta);
	Prepare_SolvedPoseStart();

	_desc.debugCollisionHitCount = 0;
	_desc.debugSegmentCollisionHitCount = 0;
	_desc.debugMinCollisionMargin = 0.f;

	Evaluate_SegmentCollisionContacts(model, ownerWorld);
	Solve_ContactPoseChains(model, ownerWorld);
	Finalize_CollisionProjection(model, ownerWorld);
	Update_ContactReleaseStates(timeDelta);
	Write_BackSolvedPoseToModel(model, ownerWorld, isOwnerIdle);
	Commit_SolvedState();
	model->Rebuild_BoneCombinedMatrices();

	_elapsedTime += std::max(0.f, timeDelta);
	Draw_DebugChains();
}

void PhysicsModifier::Draw_DebugChains() const
{
	if (!_desc.debugDrawChains && !_desc.debugDrawNodes && !_desc.debugDrawSegments && !_desc.debugDrawCollisions)
	{
		return;
	}

	for (const auto& chain : _chains)
	{
		for (const auto& node : chain.nodes)
		{
			Vec3 nodePos = node.position;
			if (_desc.debugDrawNodes)
			{
				DebugLineDesc x{};
				x.start = nodePos + Vec3(-2.f, 0.f, 0.f);
				x.end = nodePos + Vec3(2.f, 0.f, 0.f);
				x.style.color = Color(1.f, 0.2f, 0.2f, 1.f);
				x.style.depthEnabled = false;
				GAME->Draw_DebugLine(x);

				DebugLineDesc y{};
				y.start = nodePos + Vec3(0.f, -2.f, 0.f);
				y.end = nodePos + Vec3(0.f, 2.f, 0.f);
				y.style.color = Color(0.2f, 1.f, 0.2f, 1.f);
				y.style.depthEnabled = false;
				GAME->Draw_DebugLine(y);
			}

			Vec3 parentPos = Vec3::Zero;
			if (node.parentNodeIndex >= 0)
			{
				parentPos = chain.nodes[node.parentNodeIndex].position;
			}
			else if (chain.anchorBoneIndex >= 0)
			{
				parentPos = Get_Position(chain.anchorWorld);
			}

			const bool drawCollisionSegment =
				_desc.debugDrawCollisions && (node.hasCollisionContact || node.hasCollisionPushApplied);
			if (_desc.debugDrawChains || _desc.debugDrawSegments || drawCollisionSegment)
			{
				DebugLineDesc simLine{};
				simLine.start = parentPos;
				simLine.end = nodePos;
				simLine.style.color = drawCollisionSegment
					? Color(1.f, 0.1f, 0.1f, 1.f)
					: (_desc.debugDrawSegments ? Color(1.f, 0.9f, 0.05f, 1.f) : Color(0.1f, 1.f, 0.2f, 1.f));
				simLine.style.depthEnabled = false;
				GAME->Draw_DebugLine(simLine);
			}
		}
	}
}

Shared<PhysicsModifier> PhysicsModifier::Create(const PhysicsModifierDesc* desc)
{
	Shared<PhysicsModifier> instance = make_shared<PhysicsModifier>();
	if (FAILED(instance->Initialize(const_cast<PhysicsModifierDesc*>(desc))))
	{
		assert(false && "Failed to Create PhysicsModifier");
		return nullptr;
	}

	return instance;
}

void PhysicsModifier::Free()
{
	AnimationModifier::Free();
}

NS_END
