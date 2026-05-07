#pragma once
#include "Decorator.h"
NS_BEGIN(Client)
class BT_CheckDistance :
    public Decorator
{
    GENERATED_BTNODE(BT_CheckDistance, "Decorator")

public:
    BT_CheckDistance();
    virtual ~BT_CheckDistance();
public:
    HRESULT Initialize(float distance, bool isLower);
    virtual bool Check_Condition() override;

    virtual NODE_STATE Update(float deltaTime) override;

protected:
    float _distance = {};
    bool _isLower = {};
public:
    static Shared<BT_CheckDistance> Create(float distance, bool isLower);
    virtual void Free()override;
};

NS_END
