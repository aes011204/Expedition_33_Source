#pragma once

#include "Decorator.h"

NS_BEGIN(Client)

class BT_HasDeadHero : public Decorator
{
    GENERATED_BTNODE(BT_HasDeadHero, "Decorator")

public:
    BT_HasDeadHero();
    ~BT_HasDeadHero() override = default;

public:
    bool Check_Condition() override;

public:
    static Shared<BT_HasDeadHero> Create();
    void Free() override;
};

NS_END
