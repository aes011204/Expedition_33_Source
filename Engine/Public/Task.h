#pragma once
#include "BNode.h"
//실제 실행할 행동
NS_BEGIN(Engine)

class Task :
    public BNode
{
public:
    Task();
    virtual ~Task();

public:
    void OnStart();
    virtual BNode::NODE_STATE Update(float deltaTime);
    void OnFinish();

};

NS_END
