#include "BT_SL_SetDesiredState.h"

#include "SL_Dualliste.h"
#include "AIController.h"
#include "BlackBoard.h"
#include "SL_Base.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(BT_SL_SetDesiredState)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_ENUM("Desired State", _desiredState, SL_MonState);
    PROPERTY_BBKEY_INT("Desired State Key", _desiredStateKey);

    return true;
}

BT_SL_SetDesiredState::BT_SL_SetDesiredState()
{
}

void BT_SL_SetDesiredState::OnStart()
{
}

BNode::NODE_STATE BT_SL_SetDesiredState::Logic_Update(float deltaTime)
{
    UNREFERENCED_PARAMETER(deltaTime);

    const Shared<AIController> owner = Get_Owner();
    if (owner == nullptr)
        return BNode::NODE_STATE::FAILED;

    const Shared<BlackBoard> bb = owner->Get_BlackBoard();
    if (bb == nullptr)
        return BNode::NODE_STATE::FAILED;

    const Shared<SL_Dualliste> dua = dynamic_pointer_cast<SL_Dualliste>(_owner.lock());
    if (dua == nullptr)
        return BNode::NODE_STATE::FAILED;

    if (!dua->Change_State(_desiredState))
        return BNode::NODE_STATE::FAILED;

    return BNode::NODE_STATE::SUCCESS;
}

void BT_SL_SetDesiredState::OnFinish()
{
}

Shared<BT_SL_SetDesiredState> BT_SL_SetDesiredState::Create()
{
    return make_shared<BT_SL_SetDesiredState>();
}

void BT_SL_SetDesiredState::Free()
{
    Task::Free();
}

NS_END
