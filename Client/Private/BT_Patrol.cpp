#include "BT_Patrol.h"
#include "AIController.h"
#include "BlackBoard.h"
#include "Monster_WorldBase.h"

#include <cfloat>

namespace
{
constexpr float kArriveDistSq = 0.25f;
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
    _avoidance.Reset();
    _hasRequestedTarget = false;
    _reportedUnavailable = false;
    _patrolPoints.clear();
    Shared<Monster_WorldBase> monster = dynamic_pointer_cast<Monster_WorldBase>(Get_Owner()->Get_Owner());
    if (!monster || !monster->Get_Transform())
        return;

    _patrolPoints = monster->Get_EffectivePatrolPoints();
    if (_patrolPoints.empty())
        return;

    monster->Set_DesiredAction(Monster_WorldBase::MonsterWorldAnim::Patrol);

    const Vec3 ownerPos = monster->Get_Transform()->Get_Position();
    const bool resumePatrolProgress =
        _resumePatrolProgressAfterHit &&
        _hasPatrolProgress &&
        _curIndex < static_cast<uint32>(_patrolPoints.size());

    if (!resumePatrolProgress)
    {
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
    }

    _hasPatrolProgress = true;
    _resumePatrolProgressAfterHit = false;
}

BNode::NODE_STATE BT_Patrol::Logic_Update(float deltaTime)
{
    if (_patrolPoints.empty())
    {
        Get_Owner()->Stop_Move();
        return BNode::NODE_STATE::FAILED;
    }

    Shared<Monster_WorldBase> monster = dynamic_pointer_cast<Monster_WorldBase>(Get_Owner()->Get_Owner());
    if (!monster)
    {
        Get_Owner()->Stop_Move();
        return BNode::NODE_STATE::FAILED;
    }

    const auto owner = Get_Owner();
    const auto transform = monster->Get_Transform();
    const auto blackBoard = owner->Get_BlackBoard();
    if (!transform || !blackBoard || _curIndex >= _patrolPoints.size())
    {
        owner->Stop_Move();
        return BNode::NODE_STATE::FAILED;
    }

    monster->Set_DesiredAction(Monster_WorldBase::MonsterWorldAnim::Patrol);
    const Vec3 ownerPos = transform->Get_Position();
    Vec3 toPatrolPoint = _patrolPoints[_curIndex] - ownerPos;
    toPatrolPoint.y = 0.f;
    if (toPatrolPoint.LengthSquared() <= kArriveDistSq)
    {
        _curIndex = (_curIndex + 1) % static_cast<uint32>(_patrolPoints.size());
        _avoidance.Reset();
        _hasRequestedTarget = false;
    }

    const auto result = _avoidance.Update(monster, _patrolPoints[_curIndex], deltaTime);
    blackBoard->Set_Value("PatrolPos", result.target);
    if (result.state == PatrolAvoidance::State::Unavailable)
    {
        if (!_reportedUnavailable)
            LOG_WARN("BT_Patrol: obstacle query unavailable; waiting for a valid collider.");
        _reportedUnavailable = true;
    }
    else
        _reportedUnavailable = false;
    if (result.state == PatrolAvoidance::State::Blocked ||
        result.state == PatrolAvoidance::State::Unavailable)
    {
        owner->Stop_Move();
        _hasRequestedTarget = false;
        return BNode::NODE_STATE::RUN;
    }

    if (!_hasRequestedTarget || (_requestedTarget - result.target).LengthSquared() > 0.0001f ||
        !owner->Has_MoveIntent())
    {
        Vec3 target = result.target;
        owner->Request_Move(target);
        _requestedTarget = target;
        _hasRequestedTarget = true;
    }
    return BNode::NODE_STATE::RUN;
}

void BT_Patrol::OnFinish()
{
    const Shared<AIController> owner = Get_Owner();
    Shared<Monster_WorldBase> monster = owner ? dynamic_pointer_cast<Monster_WorldBase>(owner->Get_Owner()) : nullptr;
    const bool interruptedByHit =
        monster &&
        (monster->Is_Stunned() || monster->Get_CurrentAction() == Monster_WorldBase::MonsterWorldAnim::Hit);

    _resumePatrolProgressAfterHit =
        interruptedByHit &&
        !_patrolPoints.empty() &&
        _curIndex < static_cast<uint32>(_patrolPoints.size());

    if (!_resumePatrolProgressAfterHit)
        _hasPatrolProgress = false;

    _avoidance.Reset();
    _hasRequestedTarget = false;
}

Shared<BT_Patrol> BT_Patrol::Create()
{
    return make_shared<BT_Patrol>();
}

void BT_Patrol::Free()
{
    __super::Free();
}
