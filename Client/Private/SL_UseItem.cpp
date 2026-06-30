#include "SL_UseItem.h"

#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"

NS_BEGIN(Client)

SL_UseItem::SL_UseItem(const Shared<SL_Maelle>& owner)
    : SL_State(owner)
{
}

bool SL_UseItem::EnterInvoke()
{
    Shared<SL_Maelle> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    owner->Set_CombatLocked(true);
    Stop_Movement();

    _appliedPotion = false;

    if (!Play_Anim(SoulLikeAnim::UseHeal, 0.08f, false))
    {
        owner->Set_CombatLocked(false);
        return false;
    }

    return true;
}

void SL_UseItem::Update(float timeDelta)
{
    SL_State::Update(timeDelta);

    if (!_appliedPotion)
    {
        _appliedPotion = true;
    }

    if (!Is_AnimFinished())
        return;

    if (Shared<SL_Maelle> owner = _owner.lock())
        owner->Set_CombatLocked(false);

    if (_stateCom != nullptr)
        _stateCom->Change_State(SL_PlayerState::Idle);
}

void SL_UseItem::ExitInvoke()
{
    if (Shared<SL_Maelle> owner = _owner.lock())
        owner->Set_CombatLocked(false);

    _appliedPotion = false;

    SL_State::ExitInvoke();
}

Shared<SL_UseItem> SL_UseItem::Create(const Shared<SL_Maelle>& owner)
{
    auto instance = make_shared<SL_UseItem>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_UseItem] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

NS_END
