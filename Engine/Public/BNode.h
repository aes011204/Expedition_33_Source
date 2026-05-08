#pragma once
#include"Base.h"
NS_BEGIN(Engine)
class BNode :
    public Base
{
public:
    using NODE_STATE = BTNodeState;
public:
    BNode();
    virtual ~BNode();

public:

    virtual ClassReflectionInfo& Get_ReflectionInfo()
    {
        static ClassReflectionInfo emptyInfo{};
        return emptyInfo;
    }

public:
    virtual NODE_STATE Update(float deltaTime)=0;

    void Set_Owner(weak_ptr<class AIController> aiCtrl) { _AICtrl = aiCtrl; }
    shared_ptr<class AIController> Get_Owner() { return _AICtrl.lock(); }
    NODE_STATE Get_LastState() const { return _lastState; }
    uint64 Get_AssetNodeId() const { return _assetNodeId; }
    void Set_AssetNodeId(uint64 id) { _assetNodeId = id; }
    virtual void Abort() = 0;
protected:
    NODE_STATE Commit_State(NODE_STATE state)
    {
        _lastState = state;
        return state;
    }

    weak_ptr<class AIController> _AICtrl = {};
    NODE_STATE _lastState = NODE_STATE::RUN;
    uint64 _assetNodeId = 0;
};

NS_END
