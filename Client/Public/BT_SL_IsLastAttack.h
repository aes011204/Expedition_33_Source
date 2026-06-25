#pragma once

#include "Decorator.h"
#include "SL_Types.h"

NS_BEGIN(Client)

class BT_SL_IsLastAttack final : public Decorator
{
    GENERATED_BTNODE(BT_SL_IsLastAttack, "Decorator")

public:
    BT_SL_IsLastAttack();
    ~BT_SL_IsLastAttack() override = default;

public:
    bool Check_Condition() override;

private:
    string _lastAttackKey = "LastAttack";
    SL_MonAttackType _expectedAttack = SL_MonAttackType::SingleSlash_1;
    bool _invert = false;

public:
    static Shared<BT_SL_IsLastAttack> Create();
    void Free() override;
};

NS_END
