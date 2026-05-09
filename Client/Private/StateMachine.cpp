#include "pch.h"
#include "StateMachine.h"

#include "State.h"

NS_BEGIN(Client)

StateMachine::StateMachine(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component{ device, context } {}
StateMachine::StateMachine(const StateMachine& prototype)
    : Component{ prototype } {}

HRESULT StateMachine::Initialize_Prototype()
{
    return Component::Initialize_Prototype();
}

HRESULT StateMachine::Initialize(void* arg)
{
    return Component::Initialize(arg);
}

void StateMachine::Update_StateMachine(float timeDelta)
{
    if (_currentState != nullptr)
    {
        _currentState->Update(timeDelta);
        _currentState->LateUpdate(timeDelta);
    }
}

HRESULT StateMachine::Add_State(const wstring& stateName, Shared<State> state)
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

HRESULT StateMachine::Remove_State(const wstring& stateName)
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

bool StateMachine::Change_State(const wstring& stateName)
{
    if(_states.contains(stateName))
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

Shared<StateMachine> StateMachine::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<StateMachine>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : StateMachine");
        return nullptr;
    }

    return instance;
}

Shared<Component> StateMachine::Clone(void* arg)
{
    auto clone = make_shared<StateMachine>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : StateMachine");
        return nullptr;
    }

    return clone;
}

void StateMachine::Free()
{
    _states.clear();
    _currentState.reset();

    Component::Free();
}

NS_END
