#include "BT_Patrol.h"
#include "AIController.h"
#include "BlackBoard.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "Monster_WorldBase.h"
#include "PhysX_Types.h"

#include <cfloat>

namespace
{
constexpr float kArriveDistSq = 0.25f;
constexpr float kStuckMoveEpsilonSq = 0.05f * 0.05f;
constexpr float kStuckConfirmSec = 0.4f;
constexpr float kBypassProbeDistance = 1.25f;
constexpr float kBypassForwardDistance = 0.75f;
constexpr float kBypassSideDistance = 1.75f;

Vec3 Make_FlatRight(const Vec3& forward)
{
    Vec3 right(forward.z, 0.f, -forward.x);
    if (right.LengthSquared() > 0.f)
        right.Normalize();
    return right;
}

bool Is_BlockedTowardTarget(const Shared<Monster_WorldBase>& monster, const Vec3& currentTarget)
{
    if (monster == nullptr)
        return false;

    const Shared<TransformCom> transform = monster->Get_Transform();
    const Shared<ColliderCom> collider = monster->Get_Component<ColliderCom>();
    if (transform == nullptr || collider == nullptr)
        return false;

    Vec3 toTarget = currentTarget - transform->Get_Position();
    toTarget.y = 0.f;

    const float distance = toTarget.Length();
    if (distance <= 0.75f)
        return false;

    Vec3 direction = toTarget / distance;
    PhysXQueryHit hit{};

    return GAME->Sweep_Collider(collider.get(), transform->Get_WorldMatrix(), direction, 0.8f, hit);
}

bool Try_FindBypassTarget(const Shared<Monster_WorldBase>& monster, const Vec3& currentTarget, Vec3& outBypassTarget)
{
    if (monster == nullptr)
        return false;

    const Shared<TransformCom> transform = monster->Get_Transform();
    const Shared<ColliderCom> collider = monster->Get_Component<ColliderCom>();
    if (transform == nullptr || collider == nullptr)
        return false;

    Vec3 forward = currentTarget - transform->Get_Position();
    forward.y = 0.f;
    if (forward.LengthSquared() <= 0.f)
        return false;
    forward.Normalize();

    const Vec3 right = Make_FlatRight(forward);
    if (right.LengthSquared() <= 0.f)
        return false;

    const Vec3 ownerPos = transform->Get_Position();
    const Vec3 candidates[] =
    {
        ownerPos + forward * kBypassForwardDistance - right * kBypassSideDistance,
        ownerPos + forward * kBypassForwardDistance + right * kBypassSideDistance,
    };

    float bestScore = FLT_MAX;
    bool found = false;

    for (const Vec3& candidate : candidates)
    {
        Vec3 displacement = candidate - ownerPos;
        displacement.y = 0.f;

        const float distance = displacement.Length();
        if (distance <= 0.f)
            continue;

        Vec3 direction = displacement / distance;
        PhysXQueryHit hit{};

        if (GAME->Sweep_Collider(collider.get(), transform->Get_WorldMatrix(), direction, min(distance, kBypassProbeDistance), hit))
            continue;

        const float score = (currentTarget - candidate).LengthSquared();
        if (!found || score < bestScore)
        {
            bestScore = score;
            outBypassTarget = candidate;
            found = true;
        }
    }

    return found;
}
}

IMPLEMENT_REFLECTION(BT_Patrol)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    return true;
}

BT_Patrol::BT_Patrol()
{
}

BT_Patrol::~BT_Patrol()
{
}

void BT_Patrol::OnStart()
{
    Shared<Monster_WorldBase> monster = dynamic_pointer_cast<Monster_WorldBase>(Get_Owner()->Get_Owner());
    if (!monster)
        return;

    _patrolPoints = monster->Get_EffectivePatrolPoints();
    if (_patrolPoints.empty())
        return;

    monster->Set_DesiredAction(Monster_WorldBase::MonsterWorldAnim::Patrol);

    const Vec3 ownerPos = monster->Get_Transform()->Get_Position();
    float bestDistSq = FLT_MAX;
    _curIndex = 0;

    for (uint32 i = 0; i < static_cast<uint32>(_patrolPoints.size()); ++i)
    {
        Vec3 toPoint = _patrolPoints[i] - ownerPos;
        toPoint.y = 0.f;

        const float distSq = toPoint.LengthSquared();
        if (distSq < bestDistSq)
        {
            bestDistSq = distSq;
            _curIndex = i;
        }
    }

    _lastSamplePos = ownerPos;
    _stuckAccum = 0.f;
    _usingBypassTarget = false;
    _bypassTarget = Vec3::Zero;
    _bypassAttemptCount = 0;

    Vec3 patrolTarget = _patrolPoints[_curIndex];
    Get_Owner()->Get_BlackBoard()->Set_Value("PatrolPos", patrolTarget);
    Get_Owner()->Request_Move(patrolTarget);
}

BNode::NODE_STATE BT_Patrol::Logic_Update(float deltaTime)
{
    if (_patrolPoints.empty())
        return BNode::NODE_STATE::FAILED;

    Shared<Monster_WorldBase> monster = dynamic_pointer_cast<Monster_WorldBase>(Get_Owner()->Get_Owner());
    if (!monster)
        return BNode::NODE_STATE::FAILED;

    const bool hasAuthoredPatrolPoints = !monster->Get_PatrolPoints().empty();
    monster->Set_DesiredAction(Monster_WorldBase::MonsterWorldAnim::Patrol);

    Vec3 ownerPos = monster->Get_Transform()->Get_Position();
    auto requestTarget = [&](const Vec3& target)
    {
        Vec3 mutableTarget = target;
        Get_Owner()->Get_BlackBoard()->Set_Value("PatrolPos", target);
        Get_Owner()->Request_Move(mutableTarget);
    };

    auto advancePatrolPoint = [&]()
    {
        _curIndex = (_curIndex + 1) % static_cast<uint32>(_patrolPoints.size());
        _usingBypassTarget = false;
        _bypassTarget = Vec3::Zero;
        _bypassAttemptCount = 0;
        _stuckAccum = 0.f;
        _lastSamplePos = ownerPos;
        requestTarget(_patrolPoints[_curIndex]);
    };

    const Vec3 currentTarget = _usingBypassTarget ? _bypassTarget : _patrolPoints[_curIndex];
    Vec3 toTarget = currentTarget - ownerPos;
    toTarget.y = 0.f;

    Get_Owner()->Get_BlackBoard()->Set_Value("PatrolPos", currentTarget);

    if (toTarget.LengthSquared() <= kArriveDistSq)
    {
        _stuckAccum = 0.f;
        _lastSamplePos = ownerPos;

        if (_usingBypassTarget)
        {
            _usingBypassTarget = false;
            _bypassTarget = Vec3::Zero;
            requestTarget(_patrolPoints[_curIndex]);
        }
        else
        {
            advancePatrolPoint();
        }

        return BNode::NODE_STATE::RUN;
    }

    if (!Get_Owner()->Has_MoveIntent())
        requestTarget(currentTarget); // Rotate만 남고 PathFollower intent가 끊긴 경우, 현재 patrol point로 다시 이동을 건다.

    Vec3 moved = ownerPos - _lastSamplePos;
    moved.y = 0.f;

    const bool blockedAhead = Is_BlockedTowardTarget(monster, currentTarget);

    if (!hasAuthoredPatrolPoints && Get_Owner()->Has_MoveIntent() && blockedAhead && moved.LengthSquared() <= kStuckMoveEpsilonSq)
        _stuckAccum += deltaTime;
    else
        _stuckAccum = 0.f;

    _lastSamplePos = ownerPos;

    if (_stuckAccum >= kStuckConfirmSec)
    {
        _stuckAccum = 0.f;

        if (!_usingBypassTarget && _bypassAttemptCount == 0)
        {
            Vec3 bypassTarget = Vec3::Zero;
            if (Try_FindBypassTarget(monster, _patrolPoints[_curIndex], bypassTarget))
            {
                _usingBypassTarget = true;
                _bypassTarget = bypassTarget;
                _bypassAttemptCount = 1;
                requestTarget(_bypassTarget);
            }
            else
            {
                advancePatrolPoint();
            }
        }
        else
        {
            advancePatrolPoint();
        }
    }

    return BNode::NODE_STATE::RUN;
}

void BT_Patrol::OnFinish()
{
    _stuckAccum = 0.f;
    _usingBypassTarget = false;
    _bypassTarget = Vec3::Zero;
    _bypassAttemptCount = 0;
}

Shared<BT_Patrol> BT_Patrol::Create()
{
    return make_shared<BT_Patrol>();
}

void BT_Patrol::Free()
{
    __super::Free();
}
