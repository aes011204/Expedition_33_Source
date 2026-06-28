#include "SL_Mon_PhaseChangeState.h"

#include "ClientInstance.h"
#include "SL_Dualliste.h"
#include "SL_Mon_StateMachineCom.h"
#include "SoulLikeController.h"
#include "Client_Constants.h"

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

    _phase = Phase::PhaseChange;
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
    case Phase::PhaseChange:
    {
        if (const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController())
            controller->Play_DuallisteBGM(SoulLikeController::DuallisteBGMPhase::PhaseTransition);

        Weak<SL_Dualliste> weakOwner = owner;
        const auto completePhaseChange =
            [weakOwner]()
            {
                const Shared<SL_Dualliste> owner = weakOwner.lock();
                if (owner == nullptr)
                    return;

                owner->Set_ActionLocked(false);
                owner->Finish_PhaseChange();
                owner->End_PhaseChangeTransition();
                owner->Change_State(SL_MonState::Idle);
            };

        const wstring sequencePath =
            wstring(Constants::Cinematic::RuntimeJson_Dir) +
            Constants::Cinematic::Dualliste_PhaseChange;

        CLIENT->Clear_CinematicCompletedCallbacks();
        CLIENT->Add_CinematicCompletedCallback(completePhaseChange);

        if (!CLIENT->Play_Sequence(sequencePath, 0))
        {
            CLIENT->Clear_CinematicCompletedCallbacks();
            completePhaseChange();
        }

        _phase = Phase::End;
        break;
    }

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

    switch (_phase)
    {
    case Phase::PhaseChange:
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
