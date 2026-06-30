#pragma once
#include "Player_State.h"

NS_BEGIN(Client)

class Player_WorldAction final : public Player_State
{
public:
    Player_WorldAction(const Shared<Player_WorldBase>& owner);
    ~Player_WorldAction() override = default;

public:
    HRESULT Initialize() override;
    void Begin_Play() override { Player_State::Begin_Play(); }
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

public:
    static Shared<Player_WorldAction> Create(const Shared<Player_WorldBase>& owner);
    void Free() override;
};

NS_END
