#pragma once
#include "Base.h"
#include "Client_Defines.h"
NS_BEGIN(Client)
class State;

class BStateMachine :
    public Base
{
public:
    BStateMachine();
  //  BStateMachine(const BStateMachine& prototype);
    virtual ~BStateMachine()  = default;

public:
    virtual HRESULT Initialize_Prototype() = 0;
    virtual HRESULT Initialize(void* arg) ;

public:
    void Update_StateMachine(float timeDelta);

public:
    HRESULT Add_State(const wstring& stateName, Shared<State> state);
    HRESULT Remove_State(const wstring& stateName);
    bool Change_State(const wstring& stateName);

public:
    Shared<State> Get_CurrentState() const { return _currentState; }
    const wstring& Get_CurrentStateName() const { return _currentStateName; }

protected:
    unordered_map<wstring, Shared<State>>   _states;
    Shared<State>                           _currentState{ nullptr };
    wstring                                 _currentStateName{};

public:
   // static Shared<BStateMachine> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
   // virtual Shared<Component> Clone(void* arg) = 0;
    virtual void Free() ;
};

NS_END
