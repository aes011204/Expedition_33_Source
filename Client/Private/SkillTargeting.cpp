#include "SkillTargeting.h"
#include <Battle_Event.h>

#include "BattleAnimUtils.h"
#include "GameInstance.h"
#include "BattleController.h"
#include "BattleStateMachine.h"
#include "CameraPresetTypes.h"
#include "Event_Bus.h"
#include "ClientInstance.h"

SkillTargeting::SkillTargeting(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

SkillTargeting::~SkillTargeting()
{
}

HRESULT SkillTargeting::Initialize()
{
    return Battle_State::Initialize();
}

void SkillTargeting::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool SkillTargeting::EnterInvoke()
{
    auto owner = Get_Owner();
    if (!owner)
        return false;

    const auto enemies = owner->Get_AliveEnemies();
    if (enemies.empty())
    {
        owner->Clear_TargetPreview();
        return false;
    }

    const auto& request = owner->Get_TurnActionRequest();
    const Skill_Data* skillData = CLIENT->Get_Skill(request.skillKey);
    const bool requiresTarget = Requires_SkillTarget(skillData);

    if (!requiresTarget)
    {
        owner->Ensure_EnemyTargetPreview();
        return Battle_State::EnterInvoke();
    }

    if (!owner->Ensure_EnemyTargetPreview())
        return false;

    owner->Publish_TargetPreviewEnemyFocus();
    return Battle_State::EnterInvoke();
}

void SkillTargeting::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);

    auto owner = Get_Owner();
    auto battleFSM = Get_BattleFSM();

    if (!owner || !battleFSM)
        return;

    const auto enemies = owner->Get_AliveEnemies();
    if (enemies.empty())
    {
        owner->Clear_TargetPreview();
        battleFSM->Change_State(BattleTurnState::ChooseAction);
        return;
    }

    const Skill_Data* skillData = CLIENT->Get_Skill(owner->Get_TurnActionRequest().skillKey);
    const bool requiresManualTarget = Requires_SkillTarget(skillData);

    if (GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        EvtBattlePresentation event;
        event.cameraMode = BattleCameraMode::PLAYERFOCUS;
        event.subject = owner->Get_TurnActionRequest().actor.actor;
        GAME->Get_EventBus().Publish(event);

        battleFSM->Change_State(BattleTurnState::ChooseSkill);

        auto& request = owner->Get_TurnActionRequest();
        BattleAnimUtils::Play_TurnStartOrBattleIdle(request.actor, 0.2f);

        return;
    }

    if (requiresManualTarget && GAME->KeyDown(KEY_TYPE::A))
    {
        if (owner->Move_EnemyTargetPreview(-1))
            owner->Publish_TargetPreviewEnemyFocus();
        return;
    }

    if (requiresManualTarget && GAME->KeyDown(KEY_TYPE::D))
    {
        if (owner->Move_EnemyTargetPreview(+1))
            owner->Publish_TargetPreviewEnemyFocus();
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::F))
    {
        const BattleChar* target = nullptr;

        if (requiresManualTarget)
        {
            if (!owner->Ensure_EnemyTargetPreview())
                return;

            target = owner->Get_TargetPreview();
        }
        else
        {
            owner->Ensure_EnemyTargetPreview();
            target = owner->Get_TargetPreview();

            if (target == nullptr)
                target = &enemies.front();
        }

        if (target == nullptr)
            return;

        auto& action = owner->Get_TurnActionRequest();
        action.InitTarget = *target;

        LOG_DEBUG("SkillTargeting: confirm skill={}",
            action.skillKey.c_str());

        battleFSM->Change_State(BattleTurnState::AttackPlaying);
        owner->ExecuteActionRequest(action);
        return;
    }
}

void SkillTargeting::LateUpdate(float timeDelta)
{
    Battle_State::LateUpdate(timeDelta);
}

void SkillTargeting::ExitInvoke()
{
    Battle_State::ExitInvoke();
}

bool SkillTargeting::Requires_SkillTarget(const Skill_Data* skillData)
{
    if (skillData == nullptr)
        return true;

    for (TargetType targetType : skillData->targetingType)
    {
        if (targetType == TargetType::Primary || targetType == TargetType::PrimaryJump)
            return true; // Primary 계열만 Targeting이 되도록
    }

    return false;
}

void SkillTargeting::Free()
{
    Battle_State::Free();
}

Shared<SkillTargeting> SkillTargeting::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<SkillTargeting>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}
