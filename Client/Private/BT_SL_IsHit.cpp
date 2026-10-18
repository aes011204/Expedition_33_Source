#include "BT_SL_IsHit.h"

#include "AIController.h"
#include "BlackBoard.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(BT_SL_IsHit)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BBKEY_BOOL("Is Hit Key", _isHitKey);

    return true;
}

BT_SL_IsHit::BT_SL_IsHit()
{
}

bool BT_SL_IsHit::Check_Condition()
{
    const Shared<AIController> owner = Get_Owner();
    if (owner == nullptr)
        return false;

    const Shared<BlackBoard> bb = owner->Get_BlackBoard();
    if (bb == nullptr)
        return false;

    const any* value = bb->Find_Value(_isHitKey);
    if (value == nullptr)
        return false;

    return any_cast<bool>(*value);
}

Shared<BT_SL_IsHit> BT_SL_IsHit::Create()
{
    return make_shared<BT_SL_IsHit>();
}

void BT_SL_IsHit::Free()
{
    Decorator::Free();
}

NS_END
