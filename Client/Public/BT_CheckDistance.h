#pragma once
#include "Decorator.h"
NS_BEGIN(Client)
class BT_CheckDistance :
    public Decorator
{
    GENERATED_BTNODE(BT_CheckDistance, "Decorator")

public:
    enum class EDistanceCondition
    {
        LessThan, GreaterThan, Between
    };

public:
    BT_CheckDistance();
    virtual ~BT_CheckDistance();
public:
    HRESULT Initialize(float distance, enum EDistanceCondition condition, float distance2);
    virtual bool Check_Condition() override;

    virtual NODE_STATE Update(float deltaTime) override;

protected:
    float _distance = {};
    float _distance2 = {};
    EDistanceCondition _condition = {};
public:
    static Shared<BT_CheckDistance> Create(float distance, enum EDistanceCondition condition, float distance2 = 0.f);
    virtual void Free()override;
};

NS_END
