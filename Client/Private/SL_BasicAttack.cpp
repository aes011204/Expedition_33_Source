#include "SL_BasicAttack.h"
#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"

SL_BasicAttack::SL_BasicAttack(const Shared<SL_Maelle>& owner)
    : SL_State(owner)
{
}

bool SL_BasicAttack::EnterInvoke()
{
    Shared<SL_Maelle> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    owner->Set_CombatLocked(true);

    Play_Anim(SoulLikeAnim::Attack_1, 0.05f, false);

    return true;
}

void SL_BasicAttack::Update(float timeDelta)
{
    if (!Is_AnimFinished())
        return;

    if (Shared<SL_Maelle> owner = _owner.lock())
        owner->Set_CombatLocked(false);

    if (_stateCom)
        _stateCom->Change_State(SL_PlayerState::Idle);
}

Shared<SL_BasicAttack> SL_BasicAttack::Create(const Shared<SL_Maelle>& owner)
{
    auto instance = make_shared<SL_BasicAttack>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_BASIC_ATTACK] : Failed to Create State");
        return nullptr;
    }

    return instance;
}
