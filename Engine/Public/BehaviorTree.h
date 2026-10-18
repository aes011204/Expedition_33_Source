#pragma once
#include "Base.h"
// root node 실행 , root 시작점
NS_BEGIN(Engine)

class BNode;
class AIController;

class ENGINE_DLL BehaviorTree :
    public Base
{
public:
    BehaviorTree();
    virtual ~BehaviorTree();

public:
    void Update(float deltaTime);


    void Set_Root(shared_ptr<BNode> root) { _root = root; }
    shared_ptr<BNode> Get_Root() const { return _root; }
    uint64 Get_DebugTick() const { return _debugTick; }

    void Bind_Owner(shared_ptr<AIController> owner);
    void SetOwnerRecursive(shared_ptr<BNode> root, shared_ptr<class AIController> owner);
    void SetDebugTickRecursive(shared_ptr<BNode> node, uint64 tick);
protected:
    shared_ptr<BNode> _root;
    uint64 _debugTick = 0;
    //weak_ptr<class AIController> _AICtrl;



};

NS_END
