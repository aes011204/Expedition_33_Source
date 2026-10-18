#pragma once
#include"BNode.h"
// 특정 조건일 때만 노드 실행
NS_BEGIN(Engine)
class ENGINE_DLL Decorator :
    public BNode
{


public:
    Decorator();
    virtual ~Decorator();
public:

    virtual bool Check_Condition() = 0;

    virtual NODE_STATE Update(float deltaTime);

    shared_ptr<BNode>& Get_Child() { return _child; }
    void Set_Child(shared_ptr<BNode> child) { _child = child; }
    virtual void Abort() ;
protected:
    shared_ptr<BNode> _child;

public:
    //static Shared<Decorator> Create();
    virtual void Free()override;
};

NS_END
