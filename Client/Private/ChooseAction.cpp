#include "pch.h"
#include "ChooseAction.h"

#include "Input_Manager.h"
#include "GameInstance.h"

#include "BattleController.h"
#include "BattleStateMachine.h"

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
    LOG_DEBUG("ChooseAction::EnterInvoke");

    LOG_DEBUG("PRESS F : AttackTargeting // PRESS E : ChooseSkill // PRESS W : ChooseItem");// // PRESS 4 : ChooseUltimate");

    return Battle_State::EnterInvoke();
}

void ChooseAction::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);

    auto owner = Get_Owner();
    auto battleFSM = Get_BattleFSM();

    if (!owner || !battleFSM)
        return;

    if (GAME->KeyDown(KEY_TYPE::F))
    {
        LOG_DEBUG("ChooseAction::Update: F -> BaseAttack -> AttackTargeting");
        owner->Get_TurnAction().type = EBattleContext::BaseAttack;
        battleFSM->Change_State(BattleTurnState::AttackTargeting);
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::E))
    {
        LOG_DEBUG("ChooseAction::Update: E -> Skill -> ChooseSkill");
        owner->Get_TurnAction().type = EBattleContext::Skill;
        battleFSM->Change_State(BattleTurnState::ChooseSkill);
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::W))
    {
        LOG_DEBUG("ChooseAction::Update: W -> ConsumableItem -> ChooseItem");
        owner->Get_TurnAction().type = EBattleContext::ConsumableItem;
        battleFSM->Change_State(BattleTurnState::ChooseItem);
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
