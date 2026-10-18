#pragma once
#include "StateMachineCom.h"

NS_BEGIN(Client)

class PlayerStateMachineCom final :public StateMachineCom
{
    GENERATED_COMPONENT(PlayerStateMachineCom)
public:
    PlayerStateMachineCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    PlayerStateMachineCom(const StateMachineCom& prototype);
    virtual ~PlayerStateMachineCom() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;

public:
    HRESULT Add_State(PlayerState playerState, const Shared<State>& state);
    bool Change_State(PlayerState playerState);
    Shared<State> Get_State(PlayerState playerState) const;
    void BeginPlay_RegisteredStates();
    void Refresh_BodyPlayerForStates();
    bool Reenter_CurrentState();

public:
    PlayerState Get_CurrentPlayerState() const;
    void Enter_GameMenuAnimation(uint32 introAnim, uint32 idleAnim);

private:
    bool _statesBegunPlay = false;

private:
    static Shared<PlayerStateMachineCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
