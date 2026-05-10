#pragma once
#include "Composite.h"
NS_BEGIN(Engine)

class ENGINE_DLL Selector :
    public Composite
{
    GENERATED_BTNODE(Selector, "Selector")

public:
    Selector();
    virtual ~Selector();


public:
    virtual NODE_STATE Update(float deltaTime);

    virtual void Abort()override;
private:
    Shared<BNode> _runningNode = {nullptr};
public:
    static Shared<Selector> Create();
    //virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
