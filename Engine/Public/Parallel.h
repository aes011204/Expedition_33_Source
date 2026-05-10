#pragma once
#include "Composite.h"


NS_BEGIN(Engine)
class ENGINE_DLL Parallel :
    public Composite
{

    GENERATED_BTNODE(Parallel, "Parallel")


public:
    Parallel();
    virtual ~Parallel();
public:
   
    virtual NODE_STATE Update(float deltaTime) override;
    virtual void Abort();

protected:


public:
    static Shared<Parallel> Create();
    virtual void Free() override;
};

NS_END
