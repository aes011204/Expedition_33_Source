#pragma once
#include "Battle_State.h"

NS_BEGIN(Client)

class BattleController;

class AttackTargeting : public Battle_State
{
public:
    AttackTargeting(const Shared<BattleController>& owner);
    ~AttackTargeting() override;

public:
    HRESULT Initialize() override;
    void Begin_Play() override;
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

private:

    uint32 TargetIndex = {};

public:
    void Free() override;

public:
    static Shared<AttackTargeting> Create(const Shared<BattleController>& owner);
};

NS_END
