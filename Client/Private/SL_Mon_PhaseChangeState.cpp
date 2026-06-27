#include "SL_Mon_PhaseChangeState.h"

#include "ClientInstance.h"
#include "SL_Dualliste.h"
#include "SL_Mon_StateMachineCom.h"
#include "SoulLikeController.h"

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

    _phase = Phase::StunStart;
    _hasStarted = false;

    return true;
}

void SL_Mon_PhaseChangeState::Play_CurrentPhase()
{
    const Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return;

    switch (_phase)
    {
    case Phase::StunStart:
        owner->Play_Anim(SL_Dualliste::Anim::Stun_Start, 0.1f, false);
        break;

    case Phase::StunEnd:
        owner->Play_Anim(SL_Dualliste::Anim::Stun_End, 0.1f, false);
        break;

    case Phase::PhaseChange:
        if (const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController())
            controller->Play_DuallisteBGM(SoulLikeController::DuallisteBGMPhase::PhaseTransition);

        owner->Play_Anim(SL_Dualliste::Anim::PhaseChange, 0.1f, false);
        break;

    case Phase::End:
        break;
    }
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

    if (!_hasStarted)
    {
        _hasStarted = true;
        Play_CurrentPhase();
        return;
    }

    if (!owner->Is_AnimFinished())
        return;

    switch (_phase)
    {
    case Phase::StunStart:
        _phase = Phase::StunEnd;
        Play_CurrentPhase();
        return;

    case Phase::StunEnd:
        _phase = Phase::PhaseChange;
        Play_CurrentPhase();
        return;

    case Phase::PhaseChange:
        owner->Set_ActionLocked(false);
        owner->Finish_PhaseChange();
        owner->End_PhaseChangeTransition();
        if (_stateCom->Change_State(SL_MonState::Idle))
        {
            _phase = Phase::End;
            return;
        }
        return;

    case Phase::End:
        return;
    }
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
