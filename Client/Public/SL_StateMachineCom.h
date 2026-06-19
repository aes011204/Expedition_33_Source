#pragma once

#include "Battle_Enum.h"
#include "SL_Types.h"
#include "StateMachineCom.h"

NS_BEGIN(Client)

class SL_State;

class SL_StateMachineCom : public StateMachineCom
{
    GENERATED_COMPONENT(SL_StateMachineCom)
public:
    SL_StateMachineCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    SL_StateMachineCom(const StateMachineCom& prototype);
    ~SL_StateMachineCom() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;

public:
    HRESULT Add_State(SL_PlayerState playerState, const Shared<SL_State>& state);
    bool    Change_State(SL_PlayerState playerState);
    Shared<SL_State> Get_State(SL_PlayerState playerState) const;
    Shared<SL_State> Get_CurrentSLState() const;
    SL_PlayerState Get_CurrentSoulLikeState() const;

public:
    bool Request_Move(const SL_MoveRequest& request, float timeDelta);
    bool Request_BasicAttack();
    bool Request_Dodge(const SL_MoveRequest& request);
    bool Request_Skill(const Skill_Data& skillData, uint32 slotIndex);

    void Notify_BasicAttackComboWindow(bool enabled);

private:
    Shared<class SL_Maelle> Get_OwnerMaelle() const;

private:
    static Shared<SL_StateMachineCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
