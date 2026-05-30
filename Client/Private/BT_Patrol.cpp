#include "BT_Patrol.h"
#include "AIController.h"
#include "MW_Lancelier.h"
#include "BlackBoard.h"

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

    _patrolPoints = monster->Get_PatrolPoints();

    if (_patrolPoints.empty())
        return;

    if (monster)
        monster->Set_DesiredAction(Monster_WorldBase::MonsterWorldAnim::Patrol);

    curIndex = 0;
    Get_Owner()->Get_BlackBoard()->Set_Value("PatrolPos", _patrolPoints[curIndex]);

    Get_Owner()->Request_Move(_patrolPoints[curIndex]);

}

BNode::NODE_STATE BT_Patrol::Logic_Update(float deltaTime)
{
    if(_patrolPoints.empty() == true)
        return BNode::NODE_STATE::FAILED ;

    Shared<Monster_WorldBase> monster = dynamic_pointer_cast<Monster_WorldBase>(Get_Owner()->Get_Owner());

    Vec3 OwnerPos = monster->Get_Transform()->Get_Position();

    uint32 nextIndex = (curIndex + 1) % _patrolPoints.size();

    // 지금 가야 할곳
    auto toTarget = _patrolPoints[curIndex] - OwnerPos;
    toTarget.y = 0.f;

    auto ToDir = toTarget;
    ToDir.Normalize();

    // 다음에 가야 할곳
    auto NextDir = _patrolPoints[nextIndex] - _patrolPoints[curIndex];
    NextDir.y = 0.f;
    NextDir.Normalize();

    Vec3 lookDir = ToDir + NextDir;
    lookDir.Normalize();

    Get_Owner()->Get_BlackBoard()->Set_Value("PatrolPos", _patrolPoints[curIndex]);

    if (toTarget.LengthSquared() <= 0.1f)
    {
        curIndex = nextIndex;
        Get_Owner()->Request_Move(_patrolPoints[curIndex]);
    }



    return BNode::NODE_STATE::RUN;
}

void BT_Patrol::OnFinish()
{
}

Shared<BT_Patrol> BT_Patrol::Create()
{
    auto instance = make_shared<BT_Patrol>();

    /*if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Lancelier");
        return nullptr;
    }*/

    return instance;
}

void BT_Patrol::Free()
{
    __super::Free();
}
