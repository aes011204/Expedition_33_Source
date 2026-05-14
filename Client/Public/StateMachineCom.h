#pragma once
#include "Component.h"

NS_BEGIN(Client)

class State;

class StateMachineCom : public Component
{
    GENERATED_COMPONENT(StateMachineCom)
public:
    StateMachineCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    StateMachineCom(const StateMachineCom& prototype);
    virtual ~StateMachineCom() override = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;

public:
    void Update_StateMachine(float timeDelta);
    void Update_CurrentState(float timeDelta);
    void LateUpdate_CurrentState(float timeDelta);
    
public:
    HRESULT Add_State(const wstring& stateName, Shared<State> state);
    HRESULT Remove_State(const wstring& stateName);
    bool    Change_State(const wstring& stateName);

public:
    Shared<State>  Get_CurrentState() const { return _currentState; }
    const wstring& Get_CurrentStateName() const { return _currentStateName; }

protected:
    unordered_map<wstring, Shared<State>>   _states;
    Shared<State>                           _currentState{ nullptr };
    wstring                                 _currentStateName{};

private:
    static Shared<StateMachineCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
