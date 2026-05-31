#pragma once
#include "Decorator.h"
NS_BEGIN(Client)
class BT_IsStun :
    public Decorator
{
    GENERATED_BTNODE(BT_IsStun, "Decorator")

public:
    BT_IsStun();
    virtual ~BT_IsStun();
public:
    virtual bool Check_Condition() override;

    virtual NODE_STATE Update(float deltaTime) override;

protected:

public:
    string _isStun = "isStun";
public:
    static Shared<BT_IsStun> Create();
    virtual void Free()override;
};

NS_END
