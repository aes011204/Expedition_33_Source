#include "pch.h"
#include "ChooseSkill.h"
#include <Battle_Event.h>
#include "BattleAnimutils.h"
#include "Input_Manager.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "BattleController.h"
#include "BattleStateMachine.h"
#include "CombatStatCom.h"
#include "Event_Bus.h"

ChooseSkill::ChooseSkill(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

ChooseSkill::~ChooseSkill()
{
}

HRESULT ChooseSkill::Initialize()
{
    return Battle_State::Initialize();
}

void ChooseSkill::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool ChooseSkill::EnterInvoke()
{
    auto owner = Get_Owner();
    if (!owner)
        return false;

    auto& request = owner->Get_TurnActionRequest();
    request.skillKey.clear(); // 스킬 메뉴 재진입 시 이전 선택값 제거
    request.skillPage = 0;    // 기본 스킬 페이즈는 항상 0

    const BattleChar& actor = request.actor;
    if (actor.ModelCom)
    {
        // Enter 애니메이션 진입 후
        if (!actor.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Skill_Enter), 0.2f, false))
        {
            // 없으면 fallback용 Idle로
            actor.ModelCom->Set_AnimationByState(
                ETOI(BattleAnimType::Skill_Idle), 0.2f, true
            );
        }
    }

    return Battle_State::EnterInvoke();
}

void ChooseSkill::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);

    auto owner = Get_Owner();
    auto battleFSM = Get_BattleFSM();

    if (!owner || !battleFSM)
        return;

    auto& request = owner->Get_TurnActionRequest();
    if (request.actor.statCom == nullptr)
        return;

    const auto enemies = owner->Get_AliveEnemies();

    if (GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        request.skillKey.clear(); // 스킬 메뉴 취소
        request.skillPage = 0;
        BattleAnimUtils::Play_BattleIdle(request.actor, 0.3f);

        EvtBattlePresentation event;
        event.cameraMode = BattleCameraMode::PLAYERFOCUS;
        event.subject = owner->Get_TurnActionRequest().actor.actor;
        GAME->Get_EventBus().Publish(event);

        battleFSM->Change_State(BattleTurnState::ChooseAction);
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::R))
    {
        request.skillPage = (request.skillPage == 0) ? 1 : 0;
        return;
    }

    uint32 slotIndex = 0;
    if (request.skillPage == 0)
    {
        if (GAME->KeyDown(KEY_TYPE::Q))
            slotIndex = 0;
        else if (GAME->KeyDown(KEY_TYPE::W))
            slotIndex = 1;
        else if (GAME->KeyDown(KEY_TYPE::E))
            slotIndex = 2;
        else
            return;
    }
    else
    {
        if (GAME->KeyDown(KEY_TYPE::Q))
            slotIndex = 3;
        else if (GAME->KeyDown(KEY_TYPE::W))
            slotIndex = 4;
        else
            return; // TODO : 그래디언트 추가하기.
    }

    string skillKey{};
    if (!request.actor.statCom->Try_GetSkillKeyBySlot(slotIndex, skillKey))
    {
        LOG_DEBUG("ChooseSkill: empty or missing battle skill slot. slotIndex={}", slotIndex);
        return; // 아직 해금되지 않은 슬롯은 눌러도 선택되지 않는다. 나중에는 스킬 텍스트도 안뜨게 변경 예정
    }

    const Skill_Data* skillData = CLIENT->Get_Skill(skillKey);
    if (skillData == nullptr)
    {
        LOG_ERROR("ChooseSkill: skill data not found. skillKey={}", skillKey.c_str());
        return;
    }

    const uint32 apCost = skillData->apCost > 0 ? static_cast<uint32>(skillData->apCost) : 0u;
    if (request.actor.statCom->Get_CurAP() < apCost)
    {
        LOG_DEBUG("ChooseSkill: not enough AP. skillKey={}, curAP={}, cost={}",
            skillKey.c_str(),
            request.actor.statCom->Get_CurAP(),
            apCost);
        return;
    }

    request.type = EBattleContext::Skill;
    request.skillKey = skillData->skillKey; // SkillTargeting에서 target까지 확정한 뒤 Execute_Skill로 넘긴다.

    battleFSM->Change_State(BattleTurnState::SkillTargeting);


}

void ChooseSkill::LateUpdate(float timeDelta)
{
    Battle_State::LateUpdate(timeDelta);
}

void ChooseSkill::ExitInvoke()
{
    Battle_State::ExitInvoke();
}

void ChooseSkill::Free()
{
    Battle_State::Free();
}

Shared<ChooseSkill> ChooseSkill::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<ChooseSkill>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}
