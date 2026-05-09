#pragma once
#include "Player_State.h"

NS_BEGIN(Client)

class Player_Idle final : public Player_State
{
public:
    Player_Idle(const Shared<Player>& owner);
    ~Player_Idle() override = default;

public:
    HRESULT Initialize() override;
    void Begin_Play() override { Player_State::Begin_Play(); }
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

private:
    void        Set_IdleAnimation() const;

private:
    bool        _isFirstState{ true };

public:
    static Shared<Player_Idle> Create(const Shared<Player>& owner);
    virtual void Free() override;
};

NS_END
