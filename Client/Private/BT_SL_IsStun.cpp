#include "BT_SL_IsStun.h"

#include "AIController.h"
#include "BlackBoard.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(BT_SL_IsStun)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BBKEY_BOOL("Is Stun Key", _isStunKey);

    return true;
}

BT_SL_IsStun::BT_SL_IsStun()
{
}

bool BT_SL_IsStun::Check_Condition()
{
    const Shared<AIController> owner = Get_Owner();
    if (owner == nullptr)
        return false;

    const Shared<BlackBoard> bb = owner->Get_BlackBoard();
    if (bb == nullptr)
        return false;

    const any* value = bb->Find_Value(_isStunKey);
    if (value == nullptr)
        return false;

    return any_cast<bool>(*value);
}

Shared<BT_SL_IsStun> BT_SL_IsStun::Create()
{
    return make_shared<BT_SL_IsStun>();
}

void BT_SL_IsStun::Free()
{
    Decorator::Free();
}

NS_END
