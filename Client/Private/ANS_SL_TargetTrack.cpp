#include "ANS_SL_TargetTrack.h"

#include "ClientInstance.h"
#include "GameObject.h"
#include "MovementCom.h"
#include "SL_Maelle.h"
#include "SL_TargetCom.h"
#include "SoulLikeController.h"
#include "TransformCom.h"

IMPLEMENT_REFLECTION(ANS_SL_TargetTrack)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BOOL("Cache Target At Begin", _cacheTargetAtBegin);
    PROPERTY_BOOL("Apply Position", _applyPosition);
    PROPERTY_BOOL("Apply Rotation", _applyRotation);
    PROPERTY_BOOL("Stop Movement On End", _stopMovementOnEnd);
    PROPERTY_FLOAT("Target Distance", _targetDistance, 0.f, 10.f, 0.05f);
    PROPERTY_FLOAT("Rotation Speed Deg Per Sec", _rotationSpeedDegPerSec, 0.f, 3600.f, 1.f);

    return true;
}

void ANS_SL_TargetTrack::On_Begin(const AnimNotifyContext& context)
{
    Shared<GameObject> moveTarget = Resolve_MoveTarget(context.owner);
    if (moveTarget == nullptr || moveTarget->Get_Transform() == nullptr)
        return;

    _startPos = moveTarget->Get_Transform()->Get_WorldPosition();
    _goalPos = _startPos;
    _startRot = moveTarget->Get_Transform()->Get_WorldRotationEuler();
    _goalRot = _startRot;
    _cachedLockedTarget.reset();

    Shared<GameObject> lockedTarget = Resolve_LockedTarget();
    if (lockedTarget == nullptr || lockedTarget->Get_Transform() == nullptr)
        return;

    if (_cacheTargetAtBegin)
        _cachedLockedTarget = lockedTarget;

    _goalPos = Resolve_TargetGoalPosition(moveTarget, lockedTarget);
    _goalRot = Resolve_LookAtRotation(_startPos, lockedTarget->Get_Transform()->Get_WorldPosition(), _startRot);
}

void ANS_SL_TargetTrack::On_Tick(const AnimNotifyContext& context)
{
    Shared<GameObject> moveTarget = Resolve_MoveTarget(context.owner);
    if (moveTarget == nullptr || moveTarget->Get_Transform() == nullptr)
        return;

    Shared<GameObject> lockedTarget = _cacheTargetAtBegin ? _cachedLockedTarget.lock() : Resolve_LockedTarget();
    if (lockedTarget != nullptr && lockedTarget->Get_Transform() != nullptr)
    {
        _goalPos = Resolve_TargetGoalPosition(moveTarget, lockedTarget);
        _goalRot = Resolve_LookAtRotation(
            moveTarget->Get_Transform()->Get_WorldPosition(),
            lockedTarget->Get_Transform()->Get_WorldPosition(),
            _goalRot);
    }

    if (_applyPosition)
    {
        const Vec3 desiredWorldPos = Vec3::Lerp(_startPos, _goalPos, context.notifyStateProgress);
        Apply_Position(context, moveTarget, desiredWorldPos);
    }

    if (_applyRotation)
    {
        Vec3 currentRot = moveTarget->Get_Transform()->Get_WorldRotationEuler();
        float currentYaw = currentRot.y;
        float goalYaw = _goalRot.y;

        float deltaYaw = goalYaw - currentYaw;
        while (deltaYaw > 180.f)
            deltaYaw -= 360.f;
        while (deltaYaw < -180.f)
            deltaYaw += 360.f;

        const float maxYawStep = max(_rotationSpeedDegPerSec, 0.f) * context.deltaTime;
        const float appliedYawStep = clamp(deltaYaw, -maxYawStep, maxYawStep);

        Vec3 desiredRot = currentRot;
        desiredRot.y = currentYaw + appliedYawStep;
        Apply_Rotation(moveTarget, desiredRot);
    }
}

void ANS_SL_TargetTrack::On_End(const AnimNotifyContext& context)
{
    Shared<GameObject> moveTarget = Resolve_MoveTarget(context.owner);
    if (moveTarget == nullptr || moveTarget->Get_Transform() == nullptr)
        return;

    if (_applyPosition)
        Apply_Position(context, moveTarget, _goalPos);

    if (_applyRotation)
        Apply_Rotation(moveTarget, _goalRot);

    if (!context.isPreview && _stopMovementOnEnd)
    {
        if (Shared<MovementCom> movement = moveTarget->Get_Component<MovementCom>())
            movement->Stop_HorizontalMovement();
    }
}

Shared<GameObject> ANS_SL_TargetTrack::Resolve_MoveTarget(GameObject* notifyOwner) const
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (current->Get_Component<MovementCom>())
            return current;

        if (current->Get_Owner() == nullptr)
            return current;

        current = current->Get_Owner();
    }

    return nullptr;
}

Shared<GameObject> ANS_SL_TargetTrack::Resolve_LockedTarget() const
{
    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    const Shared<SL_Maelle> player = controller ? controller->Get_Player() : nullptr;
    if (player == nullptr)
        return nullptr;

    if (player->Is_MarkDestroy() || player->Get_Transform() == nullptr)
        return nullptr;

    return player;
}

Vec3 ANS_SL_TargetTrack::Resolve_TargetGoalPosition(const Shared<GameObject>& moveTarget, const Shared<GameObject>& lockedTarget) const
{
    if (moveTarget == nullptr || lockedTarget == nullptr ||
        moveTarget->Get_Transform() == nullptr || lockedTarget->Get_Transform() == nullptr)
    {
        return _goalPos;
    }

    const Vec3 movePos = moveTarget->Get_Transform()->Get_WorldPosition();
    const Vec3 targetPos = lockedTarget->Get_Transform()->Get_WorldPosition();

    Vec3 toTarget = targetPos - movePos;
    toTarget.y = 0.f;

    if (toTarget.LengthSquared() <= 0.0001f)
    {
        toTarget = lockedTarget->Get_Transform()->Get_WorldForward();
        toTarget.y = 0.f;
    }

    if (toTarget.LengthSquared() <= 0.0001f)
        toTarget = Vec3(0.f, 0.f, 1.f);
    else
        toTarget.Normalize();

    Vec3 goalPos = targetPos - toTarget * _targetDistance;
    goalPos.y = movePos.y;
    return goalPos;
}

Vec3 ANS_SL_TargetTrack::Resolve_LookAtRotation(const Vec3& fromWorldPos, const Vec3& toWorldPos, const Vec3& fallbackRotation) const
{
    Vec3 dir = toWorldPos - fromWorldPos;
    dir.y = 0.f;

    if (dir.LengthSquared() <= 0.0001f)
        return fallbackRotation;

    dir.Normalize();

    Vec3 rot = fallbackRotation;
    rot.y = XMConvertToDegrees(atan2f(dir.x, dir.z));
    return rot;
}

void ANS_SL_TargetTrack::Apply_Position(const AnimNotifyContext& context, const Shared<GameObject>& target, const Vec3& desiredWorldPos) const
{
    const Shared<TransformCom> transform = target ? target->Get_Transform() : nullptr;
    if (transform == nullptr)
        return;

    if (!context.isPreview)
    {
        if (Shared<MovementCom> movement = target->Get_Component<MovementCom>())
        {
            Vec3 displacement = desiredWorldPos - transform->Get_WorldPosition();
            movement->Move_ExternalDisplacement(displacement, context.deltaTime);
            return;
        }
    }

    transform->Set_WorldPosition(desiredWorldPos);
}

void ANS_SL_TargetTrack::Apply_Rotation(const Shared<GameObject>& target, const Vec3& desiredWorldRot) const
{
    const Shared<TransformCom> transform = target ? target->Get_Transform() : nullptr;
    if (transform == nullptr)
        return;

    transform->Set_WorldRotationEuler(desiredWorldRot);
}
