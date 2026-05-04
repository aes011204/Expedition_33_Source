#pragma once
#include "Composite.h"
NS_BEGIN(Engine)

class ENGINE_DLL Selector :
    public Composite
{
public:
    Selector();
    virtual ~Selector();


public:
    virtual NODE_STATE Update(float deltaTime);

   // void Add_Child(Shared<class BNode> child);

private:

public:
    static Shared<Selector> Create();
    //virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
