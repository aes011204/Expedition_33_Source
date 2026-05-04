#pragma once
#include "BNode.h"
//실제 실행할 행동
NS_BEGIN(Engine)

class ENGINE_DLL Task :
    public BNode
{
public:
    Task();
    virtual ~Task();

public:
    virtual void OnStart()=  0;
    virtual BNode::NODE_STATE Logic_Update(float deltaTime) = 0;
    virtual void OnFinish() = 0;;

    virtual BNode::NODE_STATE Update(float deltaTime);
    virtual void Abort();
private:
    bool _started = {};
};

NS_END
