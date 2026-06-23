#pragma once

#include "Decorator.h"

NS_BEGIN(Client)

class BT_SL_IsDead : public Decorator
{
    GENERATED_BTNODE(BT_SL_IsDead, "Decorator")

public:
    BT_SL_IsDead();
    ~BT_SL_IsDead() override = default;

public:
    bool Check_Condition() override;

private:
    string _isDeadKey = "IsDead";

public:
    static Shared<BT_SL_IsDead> Create();
    void Free() override;
};

NS_END
