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

    LOG_DEBUG("PRESS 1 : AttackTargeting // PRESS 2 : ChooseSkill // PRESS 3 : ChooseItem // PRESS 4 : ChooseUltimate");

    return Battle_State::EnterInvoke();
}

void ChooseAction::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);

    auto owner = Get_Owner();
    auto battleFSM = Get_BattleFSM();

    if (!owner || !battleFSM)
        return;
    if (GAME->KeyDown(KEY_TYPE::KEY_1))
    {
        LOG_DEBUG("ChooseAction::Update: KEY_1 -> BaseAttack -> AttackTargeting");
        owner->Get_TurnAction().type = EBattleContext::BaseAttack;
        battleFSM->Change_State(BattleTurnState::AttackTargeting);
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::KEY_2))
    {
        LOG_DEBUG("ChooseAction::Update: KEY_2 -> Skill -> ChooseSkill");
        owner->Get_TurnAction().type = EBattleContext::Skill;
        battleFSM->Change_State(BattleTurnState::ChooseSkill);
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::KEY_3))
    {
        LOG_DEBUG("ChooseAction::Update: KEY_3 -> ConsumableItem -> ChooseItem");
        owner->Get_TurnAction().type = EBattleContext::ConsumableItem;
        battleFSM->Change_State(BattleTurnState::ChooseItem);
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::KEY_4))
    {
        LOG_DEBUG("ChooseAction::Update: KEY_4 -> Skill -> ChooseUltimate");
        owner->Get_TurnAction().type = EBattleContext::Skill;
        battleFSM->Change_State(BattleTurnState::ChooseUltimate);
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
