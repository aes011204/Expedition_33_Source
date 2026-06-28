#include "SL_Jump.h"

#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"

namespace
{
    constexpr float kJumpInvincibleRefreshSec = 0.1f;
}

SL_Jump::SL_Jump(const Shared<SL_Maelle>& owner)
    : SL_State(owner)
{
}

bool SL_Jump::EnterInvoke()
{
    Shared<SL_Maelle> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    owner->Set_CombatLocked(true);
    owner->Set_InvincibleTime(kJumpInvincibleRefreshSec);

    Stop_Movement();
    Play_Anim(SoulLikeAnim::Jump, 0.05f, false);

    return true;
}

void SL_Jump::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    Shared<SL_Maelle> owner = _owner.lock();

    if (!Is_AnimFinished())
    {
        if (owner)
            owner->Set_InvincibleTime(kJumpInvincibleRefreshSec);

        return;
    }

    if (owner)
    {
        owner->Set_CombatLocked(false);
        owner->Set_InvincibleTime(0.f);
    }

    if (_stateCom)
        _stateCom->Change_State(SL_PlayerState::Idle);
}

void SL_Jump::ExitInvoke()
{
    if (Shared<SL_Maelle> owner = _owner.lock())
    {
        owner->Set_CombatLocked(false);
        owner->Set_InvincibleTime(0.f);
    }

    SL_State::ExitInvoke();
}

bool SL_Jump::Request_JumpCounter()
{
    return _stateCom != nullptr && _stateCom->Change_State(SL_PlayerState::JumpCounter);
}

Shared<SL_Jump> SL_Jump::Create(const Shared<SL_Maelle>& owner)
{
    auto instance = make_shared<SL_Jump>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_JUMP] : Failed to Create State");
        return nullptr;
    }

    return instance;
}
