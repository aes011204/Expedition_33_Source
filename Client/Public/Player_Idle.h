#pragma once
#include "Player_State.h"

NS_BEGIN(Client)

class Player_Idle final : public Player_State
{
public:
    Player_Idle(const Shared<Player_WorldBase>& owner);
    ~Player_Idle() override = default;

public:
    HRESULT Initialize() override;
    void Begin_Play() override { Player_State::Begin_Play(); }
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

private:
    void    Reset_IdleTimer();
    void    Set_IdleAnimation();

private:
    bool        _isFirstState{ true };
    bool        _isRoutineIdle{ false };
    float       _idleThreshHoldTime{ 0.f };
    float       _idleElapsedTime{ 0.f };
    const float _idleBlendTime{ 0.5f };

public:
    static Shared<Player_Idle> Create(const Shared<Player_WorldBase>& owner);
    virtual void Free() override;
};

NS_END
