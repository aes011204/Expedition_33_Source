#pragma once
#include "Battle_State.h"

NS_BEGIN(Client)

class BattleController;

class ChooseItem : public Battle_State
{
public:
    ChooseItem(const Shared<BattleController>& owner);
    ~ChooseItem() override;

public:
    HRESULT Initialize() override;
    void Begin_Play() override;
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

public:
    void Free() override;

public:
    static Shared<ChooseItem> Create(const Shared<BattleController>& owner);
};

NS_END
