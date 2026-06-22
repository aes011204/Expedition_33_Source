#pragma once

#include "State.h"

NS_BEGIN(Client)

class SL_Dualliste;
class SL_Mon_StateMachineCom;

class SL_Mon_State abstract : public State
{
public:
    explicit SL_Mon_State(const Shared<SL_Dualliste>& owner);
    ~SL_Mon_State() override = default;

public:
    HRESULT Initialize() override;
    virtual void Begin_Play() override;
    virtual bool EnterInvoke() override;
    virtual void Update(float timeDelta) override;
    virtual void LateUpdate(float timeDelta) override;
    virtual void ExitInvoke() override;

protected:
    Weak<SL_Dualliste> _owner{};
    SL_Mon_StateMachineCom* _stateCom = nullptr;

public:
    void Free() override;
};

NS_END
