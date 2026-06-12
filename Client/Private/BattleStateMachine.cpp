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
        case Client::BattleTurnState::AttackPlaying:
            return "AttackPlaying";
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
        case Client::BattleTurnState::FreeAimTargeting:
            return "FreeAimTargeting";
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

HRESULT BattleStateMachine::Add_State(BattleTurnState playerState, const Shared<State>& state)
{
    string name = string(magic_enum::enum_name(playerState));
    wstring wname(name.begin(), name.end());

    return BStateMachine::Add_State(wname, state);
}

//void BattleStateMachine::Update_StateMachine(float timeDelta)
//{
//    if (_currentState != nullptr)
//    {
//        _currentState->Update(timeDelta);
//        _currentState->LateUpdate(timeDelta);
//    }
//}

bool BattleStateMachine::Change_State(BattleTurnState playerState)
{
    const auto prevState = _currentState
        ? Get_CurrentPlayerState() : BattleTurnState::None;

    const string name = string(magic_enum::enum_name(playerState));
    const wstring wname = String::ToWString(name);

    const bool changed = BStateMachine::Change_State(wname);
    const BattleTurnState nextState = Get_CurrentPlayerState();

    if (changed && prevState != nextState)
        OnBattleTurnStateChanged.Broadcast(prevState, nextState);

    return changed;
}

BattleTurnState BattleStateMachine::Get_CurrentPlayerState() const
{
    if (_currentStateName.empty())
        return BattleTurnState::None; // 아직 FSM 현재 상태가 안 잡힌 프레임은 None으로 취급한다.

    const auto str = String::ToString(_currentStateName);
    const auto state = magic_enum::enum_cast<BattleTurnState>(str);
    if (!state.has_value())
        return BattleTurnState::None; // 저장된 상태명이 enum과 안 맞아도 예외 대신 안전하게 빠진다.

    return state.value();
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
