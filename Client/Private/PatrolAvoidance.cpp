#include "PatrolAvoidance.h"
#include "Monster_WorldBase.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "PhysX_Types.h"

NS_BEGIN(Client)

namespace
{
constexpr float kProbeDistance = 2.5f;
constexpr float kWaypointDistance = 1.5f;
constexpr float kWaypointArriveSq = 0.25f;
constexpr float kClearConfirmTime = 0.25f;
constexpr float kRetryInterval = 0.25f;
constexpr float kClearance = 0.1f;
}

void PatrolAvoidance::Reset()
{
    _avoiding = false;
    _waiting = false;
    _side = 0;
    _waypoint = Vec3::Zero;
    _clearTime = 0.f;
    _retryTime = 0.f;
}

PatrolAvoidance::Result PatrolAvoidance::Update(
    const Shared<Monster_WorldBase>& monster, const Vec3& patrolTarget, float deltaTime)
{
    const auto transform = monster ? monster->Get_Transform() : nullptr;
    const auto collider = monster ? monster->Get_Component<ColliderCom>() : nullptr;
    if (!transform || !collider || !collider->Get_PxShape() || !collider->Get_PxActor())
    {
        Reset();
        return { State::Unavailable, patrolTarget };
    }

    const float elapsed = max(deltaTime, 0.f);
    if (_waiting)
    {
        _retryTime -= elapsed;
        if (_retryTime > 0.f)
            return { State::Blocked, patrolTarget };
        _waiting = false;
    }

    const Vec3 position = transform->Get_Position();
    Vec3 forward = patrolTarget - position;
    forward.y = 0.f;
    const float targetDistance = forward.Length();
    if (targetDistance <= 0.0001f)
    {
        Reset();
        return { State::Direct, patrolTarget };
    }
    forward /= targetDistance;

    auto blocked = [&](const Vec3& direction, float distance)
    {
        PhysXQueryHit hit{};
        return GAME->Sweep_Collider(collider.get(), transform->Get_WorldMatrix(),
            direction, distance + kClearance, hit);
    };

    const bool directBlocked = blocked(forward, min(targetDistance, kProbeDistance));
    if (!_avoiding && !directBlocked)
        return { State::Direct, patrolTarget };

    if (_avoiding)
    {
        _clearTime = directBlocked ? 0.f : _clearTime + elapsed;
        if (_clearTime >= kClearConfirmTime)
        {
            Reset();
            return { State::Direct, patrolTarget };
        }

        Vec3 toWaypoint = _waypoint - position;
        toWaypoint.y = 0.f;
        const float distance = toWaypoint.Length();
        if (toWaypoint.LengthSquared() > kWaypointArriveSq &&
            !blocked(toWaypoint / distance, distance))
            return { State::Avoiding, _waypoint };

        // Confirm the clear route without selecting an unnecessary detour.
        if (!directBlocked)
            return { State::Blocked, patrolTarget };
    }

    const Vec3 right(forward.z, 0.f, -forward.x);
    const float angles[] = { 35.f, 60.f, 85.f };
    const int preferredSide = _side == 0 ? -1 : _side;
    for (int pass = 0; pass < 2; ++pass)
    {
        const int side = pass == 0 ? preferredSide : -preferredSide;
        for (float angle : angles)
        {
            const float radians = angle * 0.01745329252f;
            const Vec3 direction = forward * cosf(radians) + right * (sinf(radians) * side);
            if (blocked(direction, kWaypointDistance))
                continue;

            _waypoint = position + direction * kWaypointDistance;
            _side = side;
            _avoiding = true;
            _clearTime = 0.f;
            return { State::Avoiding, _waypoint };
        }
    }

    _waiting = true;
    _retryTime = kRetryInterval;
    _clearTime = 0.f;
    return { State::Blocked, patrolTarget };
}

NS_END
