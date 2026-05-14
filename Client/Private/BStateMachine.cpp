#include "pch.h"
#include "BStateMachine.h"
#include "State.h"

BStateMachine::BStateMachine():Base()

{
}

HRESULT BStateMachine::Initialize_Prototype()
{
    return S_OK;

}

HRESULT BStateMachine::Initialize(void* arg)
{
    return S_OK;
}

void BStateMachine::Update_StateMachine(float timeDelta)
{
    if (_currentState != nullptr)
    {
        _currentState->Update(timeDelta);
        _currentState->LateUpdate(timeDelta);
    }
}

HRESULT BStateMachine::Add_State(const wstring& stateName, Shared<State> state)
{
    if (state == nullptr)
    {
        LOG_ERROR("[StateMachine] : Failed to Add State");
        return E_FAIL;
    }

    if (_states.contains(stateName))
    {
        LOG_ERROR("[StateMachine] : Failed to Add State");
        return E_FAIL;
    }

    _states.emplace(stateName, state);

    return S_OK;
}

HRESULT BStateMachine::Remove_State(const wstring& stateName)
{
    auto iter = _states.find(stateName);
    if (iter == _states.end())
    {
        LOG_ERROR("[StateMachine] : Failed to Remove State - State not found");
        return E_FAIL;
    }

    if (_currentStateName == stateName || _currentState == iter->second)
    {
        LOG_ERROR("[StateMachine] : Failed to Remove State - Cannot remove currently active state");
        return E_FAIL;
    }

    _states.erase(iter);

    return S_OK;
}

bool BStateMachine::Change_State(const wstring& stateName)
{
    if (_states.contains(stateName))
    {
        if (_states[stateName]->EnterInvoke())
        {
            if (_currentState != nullptr)
            {
                _currentState->ExitInvoke();
            }

            _currentState = _states[stateName];
            _currentStateName = stateName;
        }

        return true;
    }

    return false;
}





void BStateMachine::Free()
{
    _states.clear();
    _currentState.reset();

    __super::Free();
}
