#pragma once

#include "Battle_State.h"

NS_BEGIN(Client)

class AttackPlaying final : public Battle_State
{
public:
    AttackPlaying(const Shared<BattleController>& owner);
    ~AttackPlaying() override = default;

public:
    HRESULT Initialize() override;
    void    Begin_Play() override;

    bool    EnterInvoke() override;
    void    Update(float timeDelta) override;
    void    LateUpdate(float timeDelta) override;
    void    ExitInvoke() override;

public:
    static Shared<AttackPlaying> Create(const Shared<BattleController>& owner);
    void Free() override;
};

NS_END
