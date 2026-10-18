#pragma once
#include "Composite.h"

NS_BEGIN(Engine)

class ENGINE_DLL Parallel : public Composite
{
    GENERATED_BTNODE(Parallel, "Parallel")

public:
    Parallel();
    ~Parallel() override;
public:
    NODE_STATE Update(float deltaTime) override;
    void Abort() override;

protected:
public:
    static Shared<Parallel> Create();
    void Free() override;
};

NS_END
