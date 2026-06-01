#include "BT_Patrol.h"
#include "AIController.h"
#include "BlackBoard.h"
#include "Monster_WorldBase.h"

#include <cfloat>

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

    Vec3 patrolTarget = _patrolPoints[_curIndex];
    Get_Owner()->Get_BlackBoard()->Set_Value("PatrolPos", patrolTarget);
    Get_Owner()->Request_Move(patrolTarget);
}

BNode::NODE_STATE BT_Patrol::Logic_Update(float deltaTime)
{
    UNREFERENCED_PARAMETER(deltaTime);

    if (_patrolPoints.empty())
        return BNode::NODE_STATE::FAILED;

    Shared<Monster_WorldBase> monster = dynamic_pointer_cast<Monster_WorldBase>(Get_Owner()->Get_Owner());
    if (!monster)
        return BNode::NODE_STATE::FAILED;

    monster->Set_DesiredAction(Monster_WorldBase::MonsterWorldAnim::Patrol);

    Vec3 ownerPos = monster->Get_Transform()->Get_Position();
    Vec3 toTarget = _patrolPoints[_curIndex] - ownerPos;
    toTarget.y = 0.f;

    Get_Owner()->Get_BlackBoard()->Set_Value("PatrolPos", _patrolPoints[_curIndex]);

    if (toTarget.LengthSquared() <= 0.1f)
    {
        _curIndex = (_curIndex + 1) % static_cast<uint32>(_patrolPoints.size());

        Vec3 nextTarget = _patrolPoints[_curIndex];
        Get_Owner()->Get_BlackBoard()->Set_Value("PatrolPos", nextTarget);
        Get_Owner()->Request_Move(nextTarget);
    }

    return BNode::NODE_STATE::RUN;
}

void BT_Patrol::OnFinish()
{
}

Shared<BT_Patrol> BT_Patrol::Create()
{
    return make_shared<BT_Patrol>();
}

void BT_Patrol::Free()
{
    __super::Free();
}
