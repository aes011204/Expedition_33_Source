#include "SL_JumpCounter.h"

#include "ClientInstance.h"
#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"
#include "SoulLikeController.h"

SL_JumpCounter::SL_JumpCounter(const Shared<SL_Maelle>& owner)
    : SL_State(owner)
{
}

bool SL_JumpCounter::EnterInvoke()
{
    Shared<SL_Maelle> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    owner->Set_CombatLocked(true);

    Stop_Movement();
    Play_Anim(SoulLikeAnim::Jump_Counter, 0.05f, false);

    return true;
}

void SL_JumpCounter::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_AnimFinished())
        return;

    if (_stateCom)
        _stateCom->Change_State(SL_PlayerState::Idle);
}

void SL_JumpCounter::ExitInvoke()
{
    if (Shared<SL_Maelle> owner = _owner.lock())
        owner->Set_CombatLocked(false);

    if (const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController())
        controller->Set_JumpCounterWindowOpen(false);

    SL_State::ExitInvoke();
}

Shared<SL_JumpCounter> SL_JumpCounter::Create(const Shared<SL_Maelle>& owner)
{
    auto instance = make_shared<SL_JumpCounter>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_JUMPCOUNTER] : Failed to Create State");
        return nullptr;
    }

    return instance;
}
