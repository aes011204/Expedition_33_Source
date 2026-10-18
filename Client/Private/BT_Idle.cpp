#include "BT_Idle.h"
#include "AIController.h"
#include "MW_Lancelier.h"

BT_Idle::BT_Idle()
{
}

BT_Idle::~BT_Idle()
{
}

void BT_Idle::OnStart()
{
    Get_Owner()->Stop_Move();
    Shared<Monster_WorldBase> monster = dynamic_pointer_cast<Monster_WorldBase>(Get_Owner()->Get_Owner());
    //LOG_DEBUG("BT_Idle::OnStart");
    if (monster)
        monster->Set_DesiredAction(Monster_WorldBase::MonsterWorldAnim::Idle);
}

BNode::NODE_STATE BT_Idle::Logic_Update(float deltaTime)
{
    return BNode::NODE_STATE::RUN;
}

void BT_Idle::OnFinish()
{
}

Shared<BT_Idle> BT_Idle::Create()
{
    auto instance = make_shared<BT_Idle>();

    /*if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Lancelier");
        return nullptr;
    }*/

    return instance;
}

void BT_Idle::Free()
{
    __super::Free();
}
