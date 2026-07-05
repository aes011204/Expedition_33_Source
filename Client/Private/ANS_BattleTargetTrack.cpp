#include "ANS_BattleTargetTrack.h"

#include "BattleController.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"
#include "GameObject.h"
#include "TransformCom.h"

#include <unordered_map>

NS_BEGIN(Client)

namespace
{
    unordered_map<GameObject*, Vec3> g_battleTargetTrackStartRotations;
}

IMPLEMENT_REFLECTION(ANS_BattleTargetTrack)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BOOL("Yaw Rotate To Target", _yawRotateToTarget);
    PROPERTY_BOOL("Cache Target At Begin", _cacheTargetAtBegin);
    PROPERTY_BOOL("Use World Rotation", _useWorldRotation);

    return true;
}

void ANS_BattleTargetTrack::On_Begin(const AnimNotifyContext& context)
{
    Shared<GameObject> moveTarget = Resolve_MoveTarget(context.owner);
    if (moveTarget == nullptr || moveTarget->Get_Transform() == nullptr)
        return;

    _startRot = _useWorldRotation
        ? moveTarget->Get_Transform()->Get_WorldRotationEuler()
        : moveTarget->Get_Transform()->Get_RotationEuler();
    _goalRot = _startRot;
    _cachedBattleTarget.reset();

    if (!_yawRotateToTarget)
    {
        if (const auto found = g_battleTargetTrackStartRotations.find(moveTarget.get());
            found != g_battleTargetTrackStartRotations.end())
        {
            _goalRot = found->second;
        }

        return;
    }

    g_battleTargetTrackStartRotations[moveTarget.get()] = _startRot;

    Shared<GameObject> battleTarget = Resolve_BattleTarget();
    if (battleTarget == nullptr || battleTarget->Get_Transform() == nullptr)
        return;

    if (_cacheTargetAtBegin)
        _cachedBattleTarget = battleTarget;

    _goalRot = Resolve_LookAtRotation(
        moveTarget->Get_Transform()->Get_WorldPosition(),
        battleTarget->Get_Transform()->Get_WorldPosition(),
        _startRot);
}

void ANS_BattleTargetTrack::On_Tick(const AnimNotifyContext& context)
{
    Shared<GameObject> moveTarget = Resolve_MoveTarget(context.owner);
    if (moveTarget == nullptr || moveTarget->Get_Transform() == nullptr)
        return;

    if (_yawRotateToTarget)
    {
        Shared<GameObject> battleTarget = _cacheTargetAtBegin ? _cachedBattleTarget.lock() : Resolve_BattleTarget();
        if (battleTarget == nullptr || battleTarget->Get_Transform() == nullptr)
            return;

        _goalRot = Resolve_LookAtRotation(
            moveTarget->Get_Transform()->Get_WorldPosition(),
            battleTarget->Get_Transform()->Get_WorldPosition(),
            _goalRot);
    }

    Vec3 desiredRot = Resolve_LerpedRotation(_startRot, _goalRot, context.notifyStateProgress);
    Apply_Rotation(moveTarget, desiredRot);
}

void ANS_BattleTargetTrack::On_End(const AnimNotifyContext& context)
{
    Shared<GameObject> moveTarget = Resolve_MoveTarget(context.owner);
    if (moveTarget == nullptr || moveTarget->Get_Transform() == nullptr)
        return;

    Apply_Rotation(moveTarget, _goalRot);

    if (!_yawRotateToTarget)
        g_battleTargetTrackStartRotations.erase(moveTarget.get());
}

Shared<GameObject> ANS_BattleTargetTrack::Resolve_MoveTarget(GameObject* notifyOwner) const
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (current->Get_Owner() == nullptr)
            return current;

        current = current->Get_Owner();
    }

    return nullptr;
}

Shared<GameObject> ANS_BattleTargetTrack::Resolve_BattleTarget() const
{
    const Shared<BattleController> battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return nullptr;

    const auto& curTargets = battle->Get_TurnAction().GetCurStepTarget();
    for (const BattleChar& target : curTargets)
    {
        if (target.actor != nullptr &&
            target.statCom != nullptr &&
            target.statCom->Get_AliveStatus() == EAliveStatus::Alive)
        {
            return target.actor;
        }
    }

    const BattleChar& initTarget = battle->Get_TurnAction().GetInitTarget();
    return initTarget.actor;
}

Vec3 ANS_BattleTargetTrack::Resolve_LookAtRotation(
    const Vec3& fromWorldPos,
    const Vec3& toWorldPos,
    const Vec3& fallbackRotation) const
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

Vec3 ANS_BattleTargetTrack::Resolve_LerpedRotation(const Vec3& fromRot, const Vec3& toRot, float t) const
{
    Vec3 result = fromRot;

    float deltaYaw = toRot.y - fromRot.y;
    while (deltaYaw > 180.f)
        deltaYaw -= 360.f;
    while (deltaYaw < -180.f)
        deltaYaw += 360.f;

    result.y = fromRot.y + deltaYaw * clamp(t, 0.f, 1.f);
    return result;
}

void ANS_BattleTargetTrack::Apply_Rotation(const Shared<GameObject>& target, const Vec3& desiredRot) const
{
    const Shared<TransformCom> transform = target ? target->Get_Transform() : nullptr;
    if (transform == nullptr)
        return;

    if (_useWorldRotation)
        transform->Set_WorldRotationEuler(desiredRot);
    else
        transform->Set_RotationEuler(desiredRot);
}

NS_END
