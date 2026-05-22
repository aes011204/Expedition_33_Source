#pragma once
#include "Player_State.h"

NS_BEGIN(Client)

class Player_GameMenu final : public Player_State
{
public:
    Player_GameMenu(const Shared<Player_WorldBase>& owner);
    ~Player_GameMenu() override = default;

public:
    HRESULT Initialize() override;
    void Begin_Play() override { Player_State::Begin_Play(); }
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

public:
    void Change_GameMenuAnimation(uint32 introAnim, uint32 idleAnim);

private:
    bool    _isPlayingIntro{ false };
    uint32  _pendingAnimState{ 0 };

public:
    static Shared<Player_GameMenu> Create(const Shared<Player_WorldBase>& owner);
    virtual void Free() override;
};

NS_END
