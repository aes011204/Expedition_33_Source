#include "pch.h"
#include "ChooseGradient.h"
#include "BattleAnimUtils.h"
#include "BattleController.h"
#include "BattleStateMachine.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"
#include "Event_Bus.h"
#include "GameInstance.h"
#include "GradientSkill.h"
#include "Input_Manager.h"
#include <Battle_Event.h>

NS_BEGIN(Client)

ChooseGradient::ChooseGradient(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

ChooseGradient::~ChooseGradient()
{
}

HRESULT ChooseGradient::Initialize()
{
    return Battle_State::Initialize();
}

void ChooseGradient::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool ChooseGradient::EnterInvoke()
{
    auto owner = Get_Owner();
    if (!owner)
        return false;

    auto& request = owner->Get_TurnActionRequest();
    request.skillKey.clear(); // Clear stale skill before opening the Gradient menu.

    const BattleChar& actor = request.actor;
    if (actor.ModelCom)
    {
        if (!actor.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Skill_Enter), 0.2f, false))
            actor.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Skill_Idle), 0.2f, true);
    }

    return Battle_State::EnterInvoke();
}

void ChooseGradient::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);

    auto owner = Get_Owner();
    auto battleFSM = Get_BattleFSM();
    if (!owner || !battleFSM)
        return;

    auto& request = owner->Get_TurnActionRequest();
    if (request.actor.statCom == nullptr)
        return;

    if (GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        request.skillKey.clear(); // Cancel Gradient selection.
        BattleAnimUtils::Play_BattleIdle(request.actor, 0.3f);

        EvtBattlePresentation event;
        event.cameraMode = BattleCameraMode::PLAYERFOCUS;
        event.subject = request.actor.actor;
        GAME->Get_EventBus().Publish(event);

        battleFSM->Change_State(BattleTurnState::ChooseAction);
        return;
    }

    if (!GAME->KeyDown(KEY_TYPE::Q))
        return;

    string skillKey{};
    if (!request.actor.statCom->Try_GetSkillKeyBySlot(GradientSkill::SlotIndex, skillKey))
        return; // Empty slot 6 means Gradient cannot be selected.

    const Skill_Data* skillData = CLIENT->Get_Skill(skillKey);
    if (skillData == nullptr)
        return;

    const uint32 apCost = skillData->apCost > 0 ? static_cast<uint32>(skillData->apCost) : 0u;
    if (request.actor.statCom->Get_CurAP() < apCost)
        return;

    request.type = EBattleContext::Skill;
    request.skillKey = skillData->skillKey; // SkillTargeting confirms the target before ExecuteActionRequest consumes it.
    battleFSM->Change_State(BattleTurnState::SkillTargeting);
}

void ChooseGradient::LateUpdate(float timeDelta)
{
    Battle_State::LateUpdate(timeDelta);
}

void ChooseGradient::ExitInvoke()
{
    Battle_State::ExitInvoke();
}

void ChooseGradient::Free()
{
    Battle_State::Free();
}

Shared<ChooseGradient> ChooseGradient::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<ChooseGradient>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

NS_END
