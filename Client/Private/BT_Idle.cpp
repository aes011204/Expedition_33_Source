#include "BT_Idle.h"
#include "AIController.h"

BT_Idle::BT_Idle()
{
}

BT_Idle::~BT_Idle()
{
}

void BT_Idle::OnStart()
{
    Get_Owner()->Stop_Move();
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
        MSG_BOX("Failed to Created : Monster");
        return nullptr;
    }*/

    return instance;
}

void BT_Idle::Free()
{
    __super::Free();
}
