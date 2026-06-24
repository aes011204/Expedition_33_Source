#pragma once

#include "Decorator.h"

NS_BEGIN(Client)

class BT_SL_IsIntroDone : public Decorator
{
    GENERATED_BTNODE(BT_SL_IsIntroDone, "Decorator")

public:
    BT_SL_IsIntroDone();
    ~BT_SL_IsIntroDone() override = default;

public:
    bool Check_Condition() override;

private:
    string _isIntroDoneKey = "IsIntroDone";

public:
    static Shared<BT_SL_IsIntroDone> Create();
    void Free() override;
};

NS_END
