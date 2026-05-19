#pragma once
#include "Player_State.h"

NS_BEGIN(Client)

class Player_Move final : public Player_State
{
public:
    Player_Move(const Shared<Player_WorldBase>& owner);
    ~Player_Move() override = default;

public:
    HRESULT Initialize() override;
    void Begin_Play() override { Player_State::Begin_Play(); }
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

private:
    bool isInvokePreSprint{ false };

public:
    static Shared<Player_Move> Create(const Shared<Player_WorldBase>& owner);
    virtual void Free() override;

};

NS_END
