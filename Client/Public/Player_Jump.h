#pragma once
#include "Player_State.h"

NS_BEGIN(Client)

class Player_Jump final : public Player_State
{
public:
    Player_Jump(const Shared<Player_WorldBase>& owner);
    ~Player_Jump() override = default;

public:
    HRESULT Initialize() override;
    void Begin_Play() override { Player_State::Begin_Play(); }
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;
    void Set_InitialFallTime(float fallTime) { _initialFallTime = fallTime > 0.f ? fallTime : 0.f; }

private:
	float _fallTime{ 0.f };
    float _initialFallTime{ 0.f };
    float _parkourProbeElapsed{ 0.f };
    bool _hasLeftGroundForParkourProbe{ false };
    bool _canProbeParkour{ false };
    bool _lowParkourProbeDone{ false };
    bool _repeatHighParkourProbe{ false };

public:
    static Shared<Player_Jump> Create(const Shared<Player_WorldBase>& owner);
    virtual void Free() override;
};

NS_END
