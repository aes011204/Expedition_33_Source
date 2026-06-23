#include "BT_SL_CanAttack.h"

#include "AIController.h"
#include "BlackBoard.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(BT_SL_CanAttack)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BBKEY_BOOL("Can Attack Key", _canAttackKey);

    return true;
}

BT_SL_CanAttack::BT_SL_CanAttack()
{
}

bool BT_SL_CanAttack::Check_Condition()
{
    const Shared<AIController> owner = Get_Owner();
    if (owner == nullptr)
        return false;

    const Shared<BlackBoard> bb = owner->Get_BlackBoard();
    if (bb == nullptr)
        return false;

    const any* value = bb->Find_Value(_canAttackKey);
    if (value == nullptr)
        return false;

    return any_cast<bool>(*value);
}

Shared<BT_SL_CanAttack> BT_SL_CanAttack::Create()
{
    return make_shared<BT_SL_CanAttack>();
}

void BT_SL_CanAttack::Free()
{
    Decorator::Free();
}

NS_END
