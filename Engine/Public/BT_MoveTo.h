#pragma once
#include "Task.h"
class ENGINE_DLL BT_MoveTo :
    public Task
{
    GENERATED_BTNODE(BT_MoveTo, "Task")

public:

    BT_MoveTo();
    virtual ~BT_MoveTo();

public:

    virtual void OnStart()override;
    virtual BNode::NODE_STATE Logic_Update(float deltaTime) override;
    virtual void OnFinish() override;

   // virtual NODE_STATE Update(float deltaTime) override;

    void Set_Target(Vec3 target) { _target = target; };
private:
    bool _started = false;


    Vec3 _target = {};

    Vec3 _prevPos = {};
protected:
    string _targetKey = "playerPos";
public:
    const std::string& Get_TargetKey() const { return _targetKey; }
    void Set_TargetKey(const std::string& key) { _targetKey = key; }
public:
    static Shared<BT_MoveTo> Create();
    virtual void Free()override;
};

