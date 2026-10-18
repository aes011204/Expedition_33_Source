#include "pch.h"
#include "ChooseAction.h"
#include "Client_Constants.h"
#include "Input_Manager.h"
#include "GameInstance.h"
#include "BattleController.h"
#include "BattleStateMachine.h"
#include "Battle_Event.h"
#include "CameraPresetTypes.h"
#include "Event_Bus.h"
#include "BattleAnimUtils.h"
#include "CombatStatCom.h"
#include "GradientSkill.h"

#include "BattleSpawnRegistry.h"
#include "BattleTurnCamera.h"

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
    auto owner = Get_Owner();
    if (!owner)
        return false;

    const BattleChar& actor = owner->Get_TurnActionRequest().actor;
    BattleAnimUtils::Play_BattleIdle(actor, 0.2f);
    owner->Ensure_EnemyTargetPreview();

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
        GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_TurnPass);
        owner->EndTurn();
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::A))
    {
        if (owner->Move_EnemyTargetPreview(-1))
            GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_SelectEnemy);

        return;
    }

    if (GAME->KeyDown(KEY_TYPE::D))
    {
        if (owner->Move_EnemyTargetPreview(+1))
            GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_SelectEnemy);

        return;
    }

    if (GAME->KeyDown(KEY_TYPE::F)) // 기본 공격
    {
        owner->Get_TurnActionRequest().type = EBattleContext::BaseAttack;

        if (!owner->Ensure_EnemyTargetPreview())
            return;

        if (battleFSM->Change_State(BattleTurnState::AttackTargeting))
            GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_TargetEnemyStart);

        return;
    }

    if (GAME->KeyDown(KEY_TYPE::C)) // ㅌㅌ
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_FleeActivation);
        owner->Request_EscapeBattle();
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::Q))
    {
        string gradientSkillKey{};

        // 그래디언트 스킬이 해금돼서 6번에 들어가있는지 확인
        if (owner->Get_TurnActionRequest().actor.statCom == nullptr ||
            !owner->Get_TurnActionRequest().actor.statCom->Try_GetSkillKeyBySlot(GradientSkill::SlotIndex, gradientSkillKey))
        {
            LOG_DEBUG("ChooseAction: gradient skill is locked or empty");
            return;
        }

        // Let the Gradient menu open even when the skill cannot be confirmed yet.
        // ChooseGradient keeps the final use gate so the card can show its disabled state.
        owner->Get_TurnActionRequest().type = EBattleContext::Skill;
        if (!battleFSM->Change_State(BattleTurnState::ChooseGradient))
            return;

        GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_SpellsMenuOpen);

        EvtBattlePresentation event;
        event.cameraMode = BattleCameraMode::SELECTSKILL;
        event.subject = owner->Get_TurnActionRequest().actor.actor;
        event.presetId = magic_enum::enum_name(owner->Get_TurnActionRequest().actor.heroId);
        event.useClip = true;
        GAME->Get_EventBus().Publish(event);

        return;
    }

    if (GAME->KeyDown(KEY_TYPE::E)) // 스킬
    {
        owner->Get_TurnActionRequest().type = EBattleContext::Skill;
        owner->Ensure_EnemyTargetPreview();
        if (!battleFSM->Change_State(BattleTurnState::ChooseSkill))
            return;

        GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_SpellsMenuOpen);

        EvtBattlePresentation event;
        event.cameraMode = BattleCameraMode::SELECTSKILL;
        event.subject = owner->Get_TurnActionRequest().actor.actor;
        event.presetId = magic_enum::enum_name(owner->Get_TurnActionRequest().actor.heroId);
        event.useClip = true;
        GAME->Get_EventBus().Publish(event);

        return;
    }

    if (GAME->KeyDown(KEY_TYPE::W)) // 아이템
    {
        owner->Get_TurnActionRequest().type = EBattleContext::ConsumableItem;
        if (!battleFSM->Change_State(BattleTurnState::ChooseItem))
            return;

        GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_ItemMenuOpen);

        EvtBattlePresentation event;
        event.cameraMode = BattleCameraMode::SELECTITEM;
        event.subject = owner->Get_TurnActionRequest().actor.actor;
        event.presetId = magic_enum::enum_name(owner->Get_TurnActionRequest().actor.heroId);
        GAME->Get_EventBus().Publish(event);
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::RBUTTON)) // 조준
    {
        if (!GAME->Is_ActiveCameraType(ObjectType::BattleTurnCamera))
            return;

        const BattleChar& actor = owner->Get_TurnActionRequest().actor;

        if (!battleFSM->Change_State(BattleTurnState::FreeAimTargeting))
            return;

        if (battleFSM->Get_CurrentPlayerState() != BattleTurnState::FreeAimTargeting)
            return;

        owner->Get_TurnActionRequest().type = EBattleContext::FreeAimShoot;

        if (const auto battleCamera = owner->Get_BattleTurnCamera())
        {
            float freeAimBaseYaw = 0.f;

            if (const auto spawnRegistry = owner->Get_BattleSpawnRegistry())
            {
                if (actor.actor != nullptr)
                {
                    if (const BattleSpawnRegistry::BattleEntry* spawnEntry = spawnRegistry->Find_SpawnEntry(actor.actor))
                        freeAimBaseYaw = spawnEntry->spawnRotation.y;
                }
            }

            battleCamera->Reset_FreeAimRotation(freeAimBaseYaw);
        }

        EvtBattlePresentation event;
        event.cameraMode = BattleCameraMode::AIM;
        event.subject = owner->Get_TurnActionRequest().actor.actor;
        //event.presetId = magic_enum::enum_name(owner->Get_TurnActionRequest().actor.heroId);
        GAME->Get_EventBus().Publish(event);

        return;
    }
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
