#include "BT_SL_SetSelectedAttack.h"

#include "AIController.h"
#include "BlackBoard.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(BT_SL_SetSelectedAttack)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_ENUM("Selected Attack", _selectedAttack, SL_MonAttackType);
    PROPERTY_BBKEY_INT("Selected Attack Key", _selectedAttackKey);

    return true;
}

BT_SL_SetSelectedAttack::BT_SL_SetSelectedAttack()
{
}

void BT_SL_SetSelectedAttack::OnStart()
{
}

BNode::NODE_STATE BT_SL_SetSelectedAttack::Logic_Update(float deltaTime)
{
    UNREFERENCED_PARAMETER(deltaTime);

    const Shared<AIController> owner = Get_Owner();
    if (owner == nullptr)
        return BNode::NODE_STATE::FAILED;

    const Shared<BlackBoard> bb = owner->Get_BlackBoard();
    if (bb == nullptr)
        return BNode::NODE_STATE::FAILED;

    bb->Set_Value(_selectedAttackKey, static_cast<int>(_selectedAttack));
    return BNode::NODE_STATE::SUCCESS;
}

void BT_SL_SetSelectedAttack::OnFinish()
{
}

Shared<BT_SL_SetSelectedAttack> BT_SL_SetSelectedAttack::Create()
{
    return make_shared<BT_SL_SetSelectedAttack>();
}

void BT_SL_SetSelectedAttack::Free()
{
    Task::Free();
}

NS_END
