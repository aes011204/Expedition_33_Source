#include "SL_Mon_PhaseChangeState.h"

#include "SL_Dualliste.h"
#include "SL_Mon_StateMachineCom.h"

NS_BEGIN(Client)

SL_Mon_PhaseChangeState::SL_Mon_PhaseChangeState(const Shared<SL_Dualliste>& owner)
    : SL_Mon_State(owner)
{
}

bool SL_Mon_PhaseChangeState::EnterInvoke()
{
    if (!SL_Mon_State::EnterInvoke())
        return false;

    const Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    owner->Begin_PhaseChangeTransition();
    owner->Set_ActionLocked(true);

    return owner->Play_Anim(SL_Dualliste::Anim::PhaseChange, 0.1f, false);
}

void SL_Mon_PhaseChangeState::Update(float timeDelta)
{
    SL_Mon_State::Update(timeDelta);

    const Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr || _stateCom == nullptr)
        return;

    if (owner->Is_Dead())
    {
        _stateCom->Change_State(SL_MonState::Dead);
        return;
    }

    if (!owner->Is_AnimFinished())
        return;

    owner->Set_ActionLocked(false);
    owner->Finish_PhaseChange();
    owner->End_PhaseChangeTransition();
    _stateCom->Change_State(SL_MonState::Idle);
}

Shared<SL_Mon_PhaseChangeState> SL_Mon_PhaseChangeState::Create(const Shared<SL_Dualliste>& owner)
{
    auto instance = make_shared<SL_Mon_PhaseChangeState>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_MON_PHASE_CHANGE] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

NS_END
