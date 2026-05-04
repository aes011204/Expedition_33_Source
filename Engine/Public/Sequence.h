#pragma once
#include "Composite.h"
NS_BEGIN(Engine)

class Sequence :
    public Composite
{
public:
    Sequence();
    virtual ~Sequence();


public:
    virtual NODE_STATE Update(float deltaTime);

  /*  void Add_Child(Shared<class BNode> child);*/

private:

//    vector<Shared<class BNode>> _children;

public:
    static Shared<Sequence> Create();
    //virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
