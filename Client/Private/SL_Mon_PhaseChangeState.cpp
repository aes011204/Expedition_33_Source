#include "SL_Mon_PhaseChangeState.h"

#include "ClientInstance.h"
#include "GameObject.h"
#include "MovementCom.h"
#include "SL_Dualliste.h"
#include "SL_Maelle.h"
#include "SL_Mon_StateMachineCom.h"
#include "SoulLikeController.h"
#include "Client_Constants.h"
#include "TransformCom.h"

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
        const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
        if (controller)
            controller->Play_DuallisteBGM(SoulLikeController::DuallisteBGMPhase::PhaseTransition);

        const Shared<SL_Maelle> player = controller ? controller->Get_Player() : nullptr;
        const bool ownerWasVisible = owner->Is_Visible();
        const bool playerWasVisible = player ? player->Is_Visible() : false;

        Weak<SL_Dualliste> weakOwner = owner;
        Weak<SL_Maelle> weakPlayer = player;
        const auto completePhaseChange =
            [weakOwner, weakPlayer, ownerWasVisible, playerWasVisible]()
            {
                const Shared<SL_Dualliste> owner = weakOwner.lock();
                if (owner == nullptr)
                    return;

                if (const Shared<GameObject> cineDualliste = CLIENT->Get_CinematicBoundActor("SL_Dualliste"))
                {
                    if (const Shared<TransformCom> cineTransform = cineDualliste->Get_Transform())
                    {
                        if (const Shared<TransformCom> ownerTransform = owner->Get_Transform())
                        {
                            ownerTransform->Set_WorldPosition(cineTransform->Get_WorldPosition());
                            ownerTransform->Set_WorldRotationEuler(cineTransform->Get_WorldRotationEuler());
                        }

                        if (const Shared<MovementCom> movement = owner->Get_MovementCom())
                            movement->Sync_RuntimePositionToTransform(); // CCT/runtime 위치를 컷신 종료 위치와 맞춘다.
                    }
                }

                owner->Set_Visible(ownerWasVisible);
                owner->Set_CinematicDriven(false);
                owner->Set_ActionLocked(false);
                owner->Finish_PhaseChange();
                owner->End_PhaseChangeTransition();
                owner->Change_State(SL_MonState::Idle);

                if (const Shared<SL_Maelle> player = weakPlayer.lock())
                {
                    player->Set_Visible(playerWasVisible);
                    player->Set_CinematicDriven(false);
                }
            };

        const wstring sequencePath =
            wstring(Constants::Cinematic::RuntimeJson_Dir) +
            Constants::Cinematic::Dualliste_PhaseChange;

        CLIENT->Clear_CinematicCompletedCallbacks();
        CLIENT->Add_CinematicCompletedCallback(completePhaseChange);

        const auto startPhaseChangeSequence =
            [weakOwner, weakPlayer, sequencePath, completePhaseChange]()
            {
                const Shared<SL_Dualliste> owner = weakOwner.lock();
                if (owner == nullptr)
                {
                    CLIENT->Clear_CinematicCompletedCallbacks();
                    completePhaseChange();
                    return;
                }

                owner->Set_CinematicDriven(true);
                owner->Set_Visible(false); // 컷신용 SL_Dualliste와 실제 전투 SL_Dualliste가 겹쳐 보이지 않게 숨긴다.

                if (const Shared<SL_Maelle> player = weakPlayer.lock())
                {
                    player->Set_CinematicDriven(true);
                    player->Set_Visible(false); // 2페 전환 컷신은 플레이어 target을 쓰지 않으므로 화면 노출만 막는다.
                }

                if (!CLIENT->Play_Sequence(sequencePath, 0))
                {
                    CLIENT->Clear_CinematicCompletedCallbacks();
                    completePhaseChange();
                }
            };

        if (!CLIENT->Request_ScreenFadeTransition(startPhaseChangeSequence))
            startPhaseChangeSequence();

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
