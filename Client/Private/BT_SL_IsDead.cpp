#include "BT_SL_IsDead.h"

#include "AIController.h"
#include "BlackBoard.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(BT_SL_IsDead)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BBKEY_BOOL("Is Dead Key", _isDeadKey);

    return true;
}

BT_SL_IsDead::BT_SL_IsDead()
{
}

bool BT_SL_IsDead::Check_Condition()
{
    const Shared<AIController> owner = Get_Owner();
    if (owner == nullptr)
        return false;

    const Shared<BlackBoard> bb = owner->Get_BlackBoard();
    if (bb == nullptr)
        return false;

    const any* value = bb->Find_Value(_isDeadKey);
    if (value == nullptr)
        return false;

    return any_cast<bool>(*value);
}

Shared<BT_SL_IsDead> BT_SL_IsDead::Create()
{
    return make_shared<BT_SL_IsDead>();
}

void BT_SL_IsDead::Free()
{
    Decorator::Free();
}

NS_END
