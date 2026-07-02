#include "pch.h"
#include "AttackTargeting.h"
#include "Input_Manager.h"
#include "GameInstance.h"
#include "BattleController.h"
#include "BattleStateMachine.h"
#include "Battle_Enum.h"
#include "ClientInstance.h"
#include "PlayerStatCom.h"
#include "Battle_Event.h"
#include "Event_Bus.h"
#include "BattleAnimUtils.h"

NS_BEGIN(Client)

AttackTargeting::AttackTargeting(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

AttackTargeting::~AttackTargeting()
{
}

HRESULT AttackTargeting::Initialize()
{
    return Battle_State::Initialize();
}

void AttackTargeting::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool AttackTargeting::EnterInvoke()
{
    LOG_DEBUG("AttackTargeting::EnterInvoke");

    auto owner = Get_Owner();
    if (!owner)
        return false;

    if (!owner->Ensure_EnemyTargetPreview())
        return false;

    owner->Publish_TargetPreviewEnemyFocus();
    return Battle_State::EnterInvoke();
}

void AttackTargeting::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);

    auto owner = Get_Owner();
    auto battleFSM = Get_BattleFSM();

    if (!owner || !battleFSM)
        return;

    const auto enemies = owner->Get_AliveEnemies();

    if (enemies.empty())
        return;

    if (GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        EvtBattlePresentation event;
        event.cameraMode = BattleCameraMode::PLAYERFOCUS;
        event.presetId = magic_enum::enum_name(owner->Get_TurnActionRequest().actor.heroId);
        event.subject = owner->Get_TurnActionRequest().actor.actor;
        GAME->Get_EventBus().Publish(event);

        auto& request = owner->Get_TurnActionRequest();
        request.itemType = BattleItemType::Invalid;

        battleFSM->Change_State(BattleTurnState::ChooseAction);
        BattleAnimUtils::Play_TurnStartOrBattleIdle(request.actor, 0.2f);

        return;
    }

    if (GAME->KeyDown(KEY_TYPE::F))
    {
        if (!owner->Ensure_EnemyTargetPreview())
            return;

        const BattleChar* target = owner->Get_TargetPreview();
        if (target == nullptr)
            return;

        auto& request = owner->Get_TurnActionRequest();
        request.InitTarget = *target;

        owner->Request_WidePresentationMode();

        auto data = CLIENT->Get_Skill(request.actor.statCom->Get_FirstSkill());
        request.skillKey = data ? data->skillKey : "";

        LOG_DEBUG("AttackTargeting: selected skill={}", request.skillKey.c_str());

        battleFSM->Change_State(BattleTurnState::AttackPlaying);
        owner->ExecuteActionRequest(request);
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::A))
    {
        if (owner->Move_EnemyTargetPreview(-1) == false)
            return;

        owner->Publish_TargetPreviewEnemyFocus();
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::D))
    {
        if (owner->Move_EnemyTargetPreview(+1) == false)
            return;

        owner->Publish_TargetPreviewEnemyFocus();
        return;
    }
}

void AttackTargeting::LateUpdate(float timeDelta)
{
    Battle_State::LateUpdate(timeDelta);
}

void AttackTargeting::ExitInvoke()
{
    LOG_DEBUG("AttackTargeting::ExitInvoke");

    //if (auto owner = Get_Owner())
    //    owner->Clear_TargetPreview();

    Battle_State::ExitInvoke();
}

void AttackTargeting::Free()
{
    Battle_State::Free();
}

Shared<AttackTargeting> AttackTargeting::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<AttackTargeting>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

NS_END
