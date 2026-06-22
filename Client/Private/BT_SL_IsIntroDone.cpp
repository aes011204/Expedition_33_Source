#include "BT_SL_IsIntroDone.h"

#include "AIController.h"
#include "BlackBoard.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(BT_SL_IsIntroDone)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BBKEY_BOOL("Is Intro Done Key", _isIntroDoneKey);

    return true;
}

BT_SL_IsIntroDone::BT_SL_IsIntroDone()
{
}

bool BT_SL_IsIntroDone::Check_Condition()
{
    const Shared<AIController> owner = Get_Owner();
    if (owner == nullptr)
        return false;

    const Shared<BlackBoard> bb = owner->Get_BlackBoard();
    if (bb == nullptr)
        return false;

    const any* value = bb->Find_Value(_isIntroDoneKey);
    if (value == nullptr)
        return false;


    bool IsSpend  = any_cast<bool>(*value);

    return IsSpend;
}

Shared<BT_SL_IsIntroDone> BT_SL_IsIntroDone::Create()
{
    return make_shared<BT_SL_IsIntroDone>();
}

void BT_SL_IsIntroDone::Free()
{
    Decorator::Free();
}

NS_END
