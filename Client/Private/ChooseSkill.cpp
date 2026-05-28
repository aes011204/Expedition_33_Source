#include "pch.h"
#include "ChooseSkill.h"
#include "Input_Manager.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "BattleController.h"
#include "BattleStateMachine.h"
#include "CombatStatCom.h"

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

    auto& action = owner->Get_TurnAction();
    action._selected_SkillKey = {}; // 스킬 메뉴 재진입 시 이전 선택값 제거

    const BattleChar& actor = action._curChar;
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

    auto& action = owner->Get_TurnAction();
    if (action._curChar.statCom == nullptr)
        return;

    if (GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        action._selected_SkillKey = {}; // 스킬 메뉴 취소

        if (action._curChar.ModelCom)
            action._curChar.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Base_Idle), 0.2f, true);

        battleFSM->Change_State(BattleTurnState::ChooseAction);
        return;
    }

    uint32 slotIndex = 0;
    if (GAME->KeyDown(KEY_TYPE::Q))
    {
        slotIndex = 0; // Q는 첫 번째 battle skill slot
    }
    else if (GAME->KeyDown(KEY_TYPE::W))
    {
        slotIndex = 1; // W는 두 번째 battle skill slot
    }
    else if (GAME->KeyDown(KEY_TYPE::E))
    {
        slotIndex = 2; // E는 세 번째 battle skill slot
    }
    else
        return;

    string skillKey{};
    if (!action._curChar.statCom->Try_GetSkillKeyBySlot(slotIndex, skillKey))
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
    if (action._curChar.statCom->Get_CurAP() < apCost)
    {
        LOG_DEBUG("ChooseSkill: not enough AP. skillKey={}, curAP={}, cost={}",
            skillKey.c_str(),
            action._curChar.statCom->Get_CurAP(),
            apCost);
        return;
    }

    action.type = EBattleContext::Skill;
     // SkillTree 기준으로 구성된 battle slot 스킬을 targeting/execution으로 넘긴다.
    action._selected_SkillKey = *skillData;

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
