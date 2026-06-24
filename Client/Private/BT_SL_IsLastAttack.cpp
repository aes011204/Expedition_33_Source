#include "BT_SL_IsLastAttack.h"

#include "AIController.h"
#include "BlackBoard.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(BT_SL_IsLastAttack)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BBKEY_INT("Last Attack Key", _lastAttackKey);
    PROPERTY_ENUM("Expected Attack", _expectedAttack, SL_MonAttackType);
    PROPERTY_BOOL("Invert", _invert);

    return true;
}

BT_SL_IsLastAttack::BT_SL_IsLastAttack()
{
}

bool BT_SL_IsLastAttack::Check_Condition()
{
    const Shared<AIController> owner = Get_Owner();
    if (owner == nullptr)
        return false;

    const Shared<BlackBoard> bb = owner->Get_BlackBoard();
    if (bb == nullptr)
        return false;

    const any* value = bb->Find_Value(_lastAttackKey);
    if (value == nullptr)
        return false;

    bool isMatched = false;

    try
    {
        const int lastAttack = any_cast<int>(*value);
        isMatched = lastAttack == static_cast<int>(_expectedAttack);
    }
    catch (const bad_any_cast&)
    {
        return false;
    }

    return _invert ? !isMatched : isMatched;
}

Shared<BT_SL_IsLastAttack> BT_SL_IsLastAttack::Create()
{
    return make_shared<BT_SL_IsLastAttack>();
}

void BT_SL_IsLastAttack::Free()
{
    Decorator::Free();
}

NS_END
