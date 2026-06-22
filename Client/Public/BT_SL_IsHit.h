#pragma once

#include "Decorator.h"

NS_BEGIN(Client)

class BT_SL_IsHit : public Decorator
{
    GENERATED_BTNODE(BT_SL_IsHit, "Decorator")

public:
    BT_SL_IsHit();
    ~BT_SL_IsHit() override = default;

public:
    bool Check_Condition() override;

private:
    string _isHitKey = "IsHit";

public:
    static Shared<BT_SL_IsHit> Create();
    void Free() override;
};

NS_END
