#include "SL_Hit.h"

#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"

SL_Hit::SL_Hit(const Shared<SL_Maelle>& owner)
    : SL_State(owner)
{
}

bool SL_Hit::EnterInvoke()
{
    Shared<SL_Maelle> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    owner->Set_CombatLocked(true);

    Play_Anim(SoulLikeAnim::Hit_Light, 0.1f, false);

    return true;
}

void SL_Hit::Update(float timeDelta)
{
    if (!Is_AnimFinished())
        return;

    Shared<SL_Maelle> owner = _owner.lock();
    if (owner == nullptr)
        return;

    if (owner->Is_Dead())
    {
        if (_stateCom)
            _stateCom->Change_State(SL_PlayerState::Dead);

        return;
    }

    owner->Set_CombatLocked(false);

    if (_stateCom)
        _stateCom->Change_State(SL_PlayerState::Idle);
}

Shared<SL_Hit> SL_Hit::Create(const Shared<SL_Maelle>& owner)
{
    auto instance = make_shared<SL_Hit>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_HIT] : Failed to Create State");
        return nullptr;
    }

    return instance;
}
