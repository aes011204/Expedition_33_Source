#pragma once

#include "Battle_Enum.h"
#include "StateMachineCom.h"

NS_BEGIN(Client)

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
    HRESULT Add_State(SoulLikePlayerState playerState, const Shared<State>& state);
    bool    Change_State(SoulLikePlayerState playerState);
    Shared<State> Get_State(SoulLikePlayerState playerState) const;
    SoulLikePlayerState Get_CurrentSoulLikeState() const;

public:
    bool Request_Move(const Vec3& worldDir, bool sprint, float timeDelta);
    bool Request_BasicAttack();
    bool Request_Dodge();
    bool Request_Skill(const Skill_Data& skillData);

private:
    Shared<class SL_Maelle> Get_OwnerMaelle() const;

private:
    static Shared<SL_StateMachineCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
