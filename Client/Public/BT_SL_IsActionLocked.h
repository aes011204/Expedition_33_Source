#pragma once

#include "Decorator.h"

NS_BEGIN(Client)

class BT_SL_IsActionLocked final : public Decorator
{
    GENERATED_BTNODE(BT_SL_IsActionLocked, "Decorator")

public:
    BT_SL_IsActionLocked();
    ~BT_SL_IsActionLocked() override = default;

public:
    bool Check_Condition() override;

private:
    string _isActionLockedKey = "IsActionLocked";
    bool _expectedValue = true;

public:
    static Shared<BT_SL_IsActionLocked> Create();
    void Free() override;
};

NS_END
