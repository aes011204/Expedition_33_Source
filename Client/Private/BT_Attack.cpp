#include "BT_Attack.h"
#include "AIController.h"
#include "MW_Lancelier.h"

BT_Attack::BT_Attack()
{
}

BT_Attack::~BT_Attack()
{
}

void BT_Attack::OnStart()
{
    LOG_DEBUG("attck");
    Get_Owner()->Stop_Move();
    Shared<MW_Lancelier> monster = dynamic_pointer_cast<MW_Lancelier>(Get_Owner()->Get_Owner());
    monster->Set_CurrentAction(MW_Lancelier::MonsterAction::Idle);
}

BNode::NODE_STATE BT_Attack::Logic_Update(float deltaTime)
{


    return BNode::NODE_STATE::RUN;
}

void BT_Attack::OnFinish()
{
}

Shared<BT_Attack> BT_Attack::Create()
{
    auto instance = make_shared<BT_Attack>();

    /*if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Lancelier");
        return nullptr;
    }*/

    return instance;
}

void BT_Attack::Free()
{
    __super::Free();
}
