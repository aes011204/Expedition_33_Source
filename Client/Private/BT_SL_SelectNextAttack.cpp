#include "BT_SL_SelectNextAttack.h"

#include <any>

#include "AIController.h"
#include "BlackBoard.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(BT_SL_SelectNextAttack)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BBKEY_INT("Selected Attack Key", _selectedAttackKey);
    PROPERTY_BBKEY_INT("Attack Chain Step Key", _attackChainStepKey);
    PROPERTY_ENUM("Attack Step 1", _attackStep1, SL_MonAttackType);
    PROPERTY_ENUM("Attack Step 2", _attackStep2, SL_MonAttackType);
    PROPERTY_ENUM("Attack Step 3", _attackStep3, SL_MonAttackType);
    PROPERTY_BOOL("Use Step 2", _useStep2);
    PROPERTY_BOOL("Use Step 3", _useStep3);

    return true;
}

BT_SL_SelectNextAttack::BT_SL_SelectNextAttack()
{
}

void BT_SL_SelectNextAttack::OnStart()
{
}

BNode::NODE_STATE BT_SL_SelectNextAttack::Logic_Update(float deltaTime)
{
    UNREFERENCED_PARAMETER(deltaTime);

    const Shared<AIController> owner = Get_Owner();
    if (owner == nullptr)
        return BNode::NODE_STATE::FAILED;

    const Shared<BlackBoard> bb = owner->Get_BlackBoard();
    if (bb == nullptr)
        return BNode::NODE_STATE::FAILED;

    try
    {
        int chainStep = 0;
        if (const any* chainValue = bb->Find_Value(_attackChainStepKey))
            chainStep = any_cast<int>(*chainValue);

        SL_MonAttackType selectedAttack = _attackStep1;
        int nextChainStep = 0;

        if (chainStep <= 0)
        {
            selectedAttack = _attackStep1;
            nextChainStep = _useStep2 ? 1 : 0;
        }
        else if (chainStep == 1 && _useStep2)
        {
            selectedAttack = _attackStep2;
            nextChainStep = _useStep3 ? 2 : 0;
        }
        else if (chainStep == 2 && _useStep3)
        {
            selectedAttack = _attackStep3;
            nextChainStep = 0;
        }
        else
        {
            selectedAttack = _attackStep1;
            nextChainStep = _useStep2 ? 1 : 0;
        }

        bb->Set_Value<int>(_selectedAttackKey, static_cast<int>(selectedAttack));
        bb->Set_Value<int>(_attackChainStepKey, nextChainStep);
        return BNode::NODE_STATE::SUCCESS;
    }
    catch (const bad_any_cast&)
    {
        return BNode::NODE_STATE::FAILED;
    }
}

void BT_SL_SelectNextAttack::OnFinish()
{
}

Shared<BT_SL_SelectNextAttack> BT_SL_SelectNextAttack::Create()
{
    return make_shared<BT_SL_SelectNextAttack>();
}

void BT_SL_SelectNextAttack::Free()
{
    Task::Free();
}

NS_END
