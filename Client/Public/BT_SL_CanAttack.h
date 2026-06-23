#pragma once

#include "Decorator.h"

NS_BEGIN(Client)

class BT_SL_CanAttack : public Decorator
{
    GENERATED_BTNODE(BT_SL_CanAttack, "Decorator")

public:
    BT_SL_CanAttack();
    ~BT_SL_CanAttack() override = default;

public:
    bool Check_Condition() override;

private:
    string _canAttackKey = "CanAttack";

public:
    static Shared<BT_SL_CanAttack> Create();
    void Free() override;
};

NS_END
