#pragma once
#include"BNode.h"
//자식 여러 개 관리, 직접 행동 XX, 자식 노드 실행
NS_BEGIN(Engine)
class ENGINE_DLL Composite :
    public BNode
{
public:
    Composite();
    virtual ~Composite();
public:
    vector<shared_ptr<BNode>>& Get_Children() { return _children; }
    void Add_Child(Shared<class BNode> child);
protected:
    vector<shared_ptr<BNode>> _children;
};

NS_END
