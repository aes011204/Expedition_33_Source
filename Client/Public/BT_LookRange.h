#pragma once
#include "Decorator.h"
NS_BEGIN(Client)

class BT_LookRange : public Decorator
{
    GENERATED_BTNODE(BT_LookRange, "Decorator")

public:
    BT_LookRange();
    ~BT_LookRange() override;
public:
    bool Check_Condition() override;

    NODE_STATE Update(float deltaTime) override;

protected:
    float _range = {};

    string _targetPosKey = "playerPos";
public:
public:
    static Shared<BT_LookRange> Create();
    void Free() override;
};

NS_END
