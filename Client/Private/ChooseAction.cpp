#include "pch.h"
#include "ChooseAction.h"
#include "Input_Manager.h"
#include "GameInstance.h"
#include "BattleController.h"
#include "BattleStateMachine.h"
#include "Battle_Event.h"
#include "CameraPresetTypes.h"
#include "Event_Bus.h"
#include <format>

NS_BEGIN(Client)

    ChooseAction::ChooseAction(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

ChooseAction::~ChooseAction()
{
}

HRESULT ChooseAction::Initialize()
{
    return Battle_State::Initialize();
}

void ChooseAction::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool ChooseAction::EnterInvoke()
{
    return Battle_State::EnterInvoke();
}

void ChooseAction::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);

    auto owner = Get_Owner();
    auto battleFSM = Get_BattleFSM();

    if (!owner || !battleFSM)
        return;

    if (GAME->KeyDown(KEY_TYPE::TAB)) // 턴 넘기기
    {
        owner->EndTurn();
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::F)) // 기본 공격
    {
        owner->Get_TurnActionRequest().type = EBattleContext::BaseAttack;

        const auto enemies = owner->Get_AliveEnemies();
        if (!enemies.empty())
        {
            EvtBattlePresentation event;
            event.cameraMode = BattleCameraMode::ENEMYFOCUS;
            event.subject = enemies[0].actor;
            GAME->Get_EventBus().Publish(event);
        }
  
        battleFSM->Change_State(BattleTurnState::AttackTargeting);
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::E)) // 스킬
    {
        owner->Get_TurnActionRequest().type = EBattleContext::Skill;
        battleFSM->Change_State(BattleTurnState::ChooseSkill);

        EvtBattlePresentation event;
        event.cameraMode = BattleCameraMode::SELECTSKILL;
        event.subject = owner->Get_TurnActionRequest().actor.actor;
        event.useClip = true;
        GAME->Get_EventBus().Publish(event);

        return;
    }

    if (GAME->KeyDown(KEY_TYPE::W)) // 아이템
    {
        owner->Get_TurnActionRequest().type = EBattleContext::ConsumableItem;
        battleFSM->Change_State(BattleTurnState::ChooseItem);

        EvtBattlePresentation event;
        event.cameraMode = BattleCameraMode::SELECTITEM;
        event.subject = owner->Get_TurnActionRequest().actor.actor;
        event.presetId = magic_enum::enum_name(owner->Get_TurnActionRequest().actor.heroId);
        GAME->Get_EventBus().Publish(event);
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::RBUTTON)) // 조준
    {
        owner->Get_TurnActionRequest().type = EBattleContext::FreeAimShoot;
        battleFSM->Change_State(BattleTurnState::FreeAimTargeting);
        return;
    }

    // 궁극기는 일단 제거
    //if (GAME->KeyDown(KEY_TYPE::KEY_4))
    //{
    //    LOG_DEBUG("ChooseAction::Update: KEY_4 -> Skill -> ChooseUltimate");
    //    owner->Get_TurnAction().type = EBattleContext::Skill;
    //    battleFSM->Change_State(BattleTurnState::ChooseUltimate);
    //    return;
    //}
}

void ChooseAction::LateUpdate(float timeDelta)
{
    Battle_State::LateUpdate(timeDelta);
}

void ChooseAction::ExitInvoke()
{
    LOG_DEBUG("ChooseAction::ExitInvoke");
    Battle_State::ExitInvoke();
}

void ChooseAction::Free()
{
    Battle_State::Free();
}

Shared<ChooseAction> ChooseAction::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<ChooseAction>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

NS_END
