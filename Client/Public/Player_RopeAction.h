#pragma once

#include "Player_State.h"

NS_BEGIN(Client)

class Player_RopeAction final : public Player_State
{
public:
    Player_RopeAction(const Shared<Player_WorldBase>& owner);
    ~Player_RopeAction() override = default;

public:
    HRESULT Initialize() override;
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

public:
    static Shared<Player_RopeAction> Create(const Shared<Player_WorldBase>& owner);
    void Free() override;

private:
    WorldInteractionSpot::WorldInteractionRequest _request{};
    Vec3 _warpStartPos = Vec3::Zero;
    bool _hasWarp = false;
};

NS_END
