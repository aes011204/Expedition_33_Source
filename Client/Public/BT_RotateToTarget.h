#pragma once
#include "Task.h"
NS_BEGIN(Client)
class BT_RotateToTarget :
    public Task
{


    GENERATED_BTNODE(BT_RotateToTarget, "Task")

public:

    BT_RotateToTarget();
    virtual ~BT_RotateToTarget();

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

    float _speed = {4.f};
protected:
    string _targetKey = "playerPos";
public:
    const std::string& Get_TargetKey() const { return _targetKey; }
    void Set_TargetKey(const std::string& key) { _targetKey = key; }
public:
    static Shared<BT_RotateToTarget> Create();
    virtual void Free()override;
};
NS_END
