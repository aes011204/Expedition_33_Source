#pragma once
#include "Composite.h"
NS_BEGIN(Engine)

class Sequence :
    public Composite
{
    GENERATED_BTNODE(Sequence, "Sequence")

public:
    Sequence();
    virtual ~Sequence();


public:
    virtual NODE_STATE Update(float deltaTime);
    virtual void Abort()override;

private:
    Shared<BNode> _runningNode = { nullptr };

public:
    static Shared<Sequence> Create();
    //virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
