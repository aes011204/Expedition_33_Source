#pragma once

#include "Decorator.h"

NS_BEGIN(Client)

class BT_SL_IsStun final : public Decorator
{
    GENERATED_BTNODE(BT_SL_IsStun, "Decorator")

public:
    BT_SL_IsStun();
    ~BT_SL_IsStun() override = default;

public:
    bool Check_Condition() override;

private:
    string _isStunKey = "IsStun";

public:
    static Shared<BT_SL_IsStun> Create();
    void Free() override;
};

NS_END
