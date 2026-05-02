#pragma once
#include"BNode.h"
// 특정 조건일 때만 노드 실행
NS_BEGIN(Engine)
class Decorator :
    public BNode
{
public:
    Decorator();
    virtual ~Decorator();
public:

    virtual bool Check_Condition() = 0;

    virtual NODE_STATE Update(float deltaTime);

    shared_ptr<BNode>& Get_Child() { return _child; }

protected:
    shared_ptr<BNode> _child;
};

NS_END
