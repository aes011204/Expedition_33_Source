#pragma once
#include"Base.h"
NS_BEGIN(Engine)
class GameObject;
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

    void Set_Owner(weak_ptr<class AIController> aiCtrl);
    shared_ptr<class AIController> Get_Owner() { return _AICtrl.lock(); }
    NODE_STATE Get_LastState() const { return _lastState; }
    void Set_CurrentDebugTick(uint64 tick) { _currentDebugTick = tick; }
    uint64 Get_LastDebugTick() const { return _lastDebugTick; }
    uint64 Get_AssetNodeId() const { return _assetNodeId; }
    void Set_AssetNodeId(uint64 id) { _assetNodeId = id; }
    virtual void Abort() = 0;
protected:
    NODE_STATE Commit_State(NODE_STATE state)
    {
        _lastState = state;
        _lastDebugTick = _currentDebugTick;
        return state;
    }

    weak_ptr<class AIController> _AICtrl = {};
    weak_ptr<class GameObject> _owner = {};
    NODE_STATE _lastState = NODE_STATE::RUN;
    uint64 _assetNodeId = 0;
    uint64 _currentDebugTick = 0;
    uint64 _lastDebugTick = 0;
};

NS_END
