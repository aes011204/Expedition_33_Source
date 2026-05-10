#include "pch.h"
#include "StateMachineCom.h"

#include "State.h"


NS_BEGIN(Client)

IMPLEMENT_REFLECTION(StateMachineCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "StateMachine";

    PROPERTY_ENUM("PlayerState", _currentState, PlayerState);

    return true;
}

StateMachineCom::StateMachineCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component{ device, context } {}
StateMachineCom::StateMachineCom(const StateMachineCom& prototype)
    : Component{ prototype } {}

HRESULT StateMachineCom::Initialize_Prototype()
{
    return Component::Initialize_Prototype();
}

HRESULT StateMachineCom::Initialize(void* arg)
{
    return Component::Initialize(arg);
}

void StateMachineCom::Update_StateMachine(float timeDelta)
{
    if (_currentState != nullptr)
    {
        _currentState->Update(timeDelta);
        _currentState->LateUpdate(timeDelta);
    }
}

HRESULT StateMachineCom::Add_State(const wstring& stateName, Shared<State> state)
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

HRESULT StateMachineCom::Remove_State(const wstring& stateName)
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

bool StateMachineCom::Change_State(const wstring& stateName)
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

Shared<StateMachineCom> StateMachineCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<StateMachineCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : StateMachine");
        return nullptr;
    }

    return instance;
}

Shared<Component> StateMachineCom::Clone(void* arg)
{
    auto clone = make_shared<StateMachineCom>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : StateMachine");
        return nullptr;
    }

    return clone;
}

void StateMachineCom::Free()
{
    _states.clear();
    _currentState.reset();

    Component::Free();
}

NS_END
