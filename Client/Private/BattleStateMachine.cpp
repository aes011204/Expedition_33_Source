#include "pch.h"
#include "BattleStateMachine.h"

#include "State.h"

namespace
{
    const char* BattleTurnStateClassName(Client::BattleTurnState state)
    {
        switch (state)
        {
        case Client::BattleTurnState::None:
            return "None";
        case Client::BattleTurnState::ChooseSkill:
            return "ChooseSkill";
        case Client::BattleTurnState::AttackTargeting:
            return "AttackTargeting";
        case Client::BattleTurnState::ChooseItem:
            return "ChooseItem";
        case Client::BattleTurnState::ChooseAction:
            return "ChooseAction";
        case Client::BattleTurnState::SkillTargeting:
            return "SkillTargeting";
        case Client::BattleTurnState::ItemTargeting:
            return "ItemTargeting";
        case Client::BattleTurnState::UltimateTargeting:
            return "UltimateTargeting";
        case Client::BattleTurnState::ChooseUltimate:
            return "ChooseUltimate";
        default:
            return "UnknownBattleState";
        }
    }
}

BattleStateMachine::BattleStateMachine()
    :BStateMachine()
{
}
//BattleStateMachine::BattleStateMachine(const BattleStateMachine& prototype)
//    : BStateMachine(prototype)
//{
//}

HRESULT BattleStateMachine::Initialize_Prototype()
{
    return S_OK;
}

HRESULT BattleStateMachine::Initialize(void* arg)
{
    return S_OK;
}

//void BattleStateMachine::Update_StateMachine(float timeDelta)
//{
//    if (_currentState != nullptr)
//    {
//        _currentState->Update(timeDelta);
//        _currentState->LateUpdate(timeDelta);
//    }
//}

HRESULT BattleStateMachine::Add_State(BattleTurnState playerState, const Shared<State>& state)
{
    const wstring& str = String::ToWString(magic_enum::enum_name(playerState).data());

    LOG_DEBUG("BattleStateMachine::Add_State: class={} ({}), ptr={}",
        BattleTurnStateClassName(playerState),
        static_cast<int>(playerState),
        static_cast<const void*>(state.get()));


    return BStateMachine::Add_State(str, state);
}

bool BattleStateMachine::Change_State(BattleTurnState playerState)
{
    const auto prevState = _currentState
        ? Get_CurrentPlayerState()
        : BattleTurnState::None;
    const wstring& str = String::ToWString(magic_enum::enum_name(playerState).data());

    LOG_DEBUG("BattleStateMachine::Change_State: {} ({}) -> {} ({})",
        BattleTurnStateClassName(prevState),
        static_cast<int>(prevState),
        BattleTurnStateClassName(playerState),
        static_cast<int>(playerState));

    return BStateMachine::Change_State(str);
}

BattleTurnState BattleStateMachine::Get_CurrentPlayerState() const
{
    auto str = String::ToString(_currentStateName);

    return magic_enum::enum_cast<BattleTurnState>(str).value();
}


Shared<BattleStateMachine> BattleStateMachine::Create()
{
    auto instance = make_shared<BattleStateMachine>();

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : StateMachine");
        return nullptr;
    }

    return instance;
}



void BattleStateMachine::Free()
{
    _states.clear();
    _currentState.reset();

    __super::Free();
}
