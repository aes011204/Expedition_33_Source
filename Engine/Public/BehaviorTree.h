#pragma once
#include "Base.h"
// root node 실행 , root 시작점
NS_BEGIN(Engine)

class BNode;
class AIController;

class BehaviorTree :
    public Base
{
public:
    BehaviorTree();
    virtual ~BehaviorTree();

public:
    void Update(float deltaTime);



    void Bind_Owner(shared_ptr<AIController> owner);
    void SetOwnerRecursive(shared_ptr<BNode> root, shared_ptr<class AIController> owner);
protected:
    shared_ptr<BNode> _root;
    //weak_ptr<class AIController> _AICtrl;

};

NS_END
