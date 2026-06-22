#include "BT_SL_IsActionLocked.h"

#include "AIController.h"
#include "BlackBoard.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(BT_SL_IsActionLocked)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BBKEY_BOOL("Is Action Locked Key", _isActionLockedKey);
    PROPERTY_BOOL("Expected Value", _expectedValue);

    return true;
}

BT_SL_IsActionLocked::BT_SL_IsActionLocked()
{
}

bool BT_SL_IsActionLocked::Check_Condition()
{
    const Shared<AIController> owner = Get_Owner();
    if (owner == nullptr)
        return false;

    const Shared<BlackBoard> bb = owner->Get_BlackBoard();
    if (bb == nullptr)
        return false;

    const any* value = bb->Find_Value(_isActionLockedKey);
    if (value == nullptr)
        return false;

    return any_cast<bool>(*value) == _expectedValue;
}

Shared<BT_SL_IsActionLocked> BT_SL_IsActionLocked::Create()
{
    return make_shared<BT_SL_IsActionLocked>();
}

void BT_SL_IsActionLocked::Free()
{
    Decorator::Free();
}

NS_END
