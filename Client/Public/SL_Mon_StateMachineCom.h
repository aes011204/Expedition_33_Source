#pragma once

#include "SL_Types.h"
#include "StateMachineCom.h"

NS_BEGIN(Client)

class SL_Dualliste;
class SL_Mon_State;

class SL_Mon_StateMachineCom : public StateMachineCom
{
    GENERATED_COMPONENT(SL_Mon_StateMachineCom)
public:
    SL_Mon_StateMachineCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    SL_Mon_StateMachineCom(const StateMachineCom& prototype);
    ~SL_Mon_StateMachineCom() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;

public:
    HRESULT Add_State(SL_MonState monState, const Shared<SL_Mon_State>& state);
    bool Change_State(SL_MonState monState);
    Shared<SL_Mon_State> Get_State(SL_MonState monState) const;
    Shared<SL_Mon_State> Get_CurrentMonState() const;
    SL_MonState Get_CurrentMonStateType() const;

private:
    Shared<SL_Dualliste> Get_OwnerDualliste() const;

private:
    static Shared<SL_Mon_StateMachineCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
