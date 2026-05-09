#include "BT_Attack.h"
#include "AIController.h"
#include "Monster.h"

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
    Shared<Monster> monster = dynamic_pointer_cast<Monster>(Get_Owner()->Get_Owner());
    monster->Set_CurrentAction(Monster::MonsterAction::Idle);
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
        MSG_BOX("Failed to Created : Monster");
        return nullptr;
    }*/

    return instance;
}

void BT_Attack::Free()
{
    __super::Free();
}
