#include "BT_Stun.h"
#include "AIController.h"
#include "MW_Lancelier.h"
#include "BlackBoard.h"

IMPLEMENT_REFLECTION(BT_Stun)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();



    return true;
}


BT_Stun::BT_Stun()
{
}

BT_Stun::~BT_Stun()
{
}

void BT_Stun::OnStart()
{
   auto monster = dynamic_pointer_cast<Monster_WorldBase>(Get_Owner()->Get_Owner());

   if (monster)
       monster->Set_DesiredAction(Monster_WorldBase::MonsterWorldAnim::Patrol);
}

BNode::NODE_STATE BT_Stun::Logic_Update(float deltaTime)
{
  /*  const auto bb = Get_Owner()->Get_BlackBoard();
    if (!bb || !bb->Has_Value(_isStun))
        return BNode::NODE_STATE::FAILED;

    const bool isStun = bb->Get_Value<bool>(_isStun);
    if (!isStun)
        return BNode::NODE_STATE::FAILED;

    if (monster && monster->Get_CurrentAction() != Monster_WorldBase::MonsterWorldAnim::Stun)
        monster->Set_CurrentAction(Monster_WorldBase::MonsterWorldAnim::Stun);

    Get_Owner()->Stop_Move();
    return BNode::NODE_STATE::RUN;*/

    return BNode::NODE_STATE::RUN;
}

void BT_Stun::OnFinish()
{
}

Shared<BT_Stun> BT_Stun::Create()
{
    auto instance = make_shared<BT_Stun>();

    

    return instance;
}

void BT_Stun::Free()
{
    __super::Free();
}
