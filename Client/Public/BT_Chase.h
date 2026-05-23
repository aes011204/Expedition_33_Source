#pragma once
#include "Task.h"

NS_BEGIN(Client)
class BT_Chase :
    public Task
{
    GENERATED_BTNODE(BT_Chase, "Task")
public:
    BT_Chase();
    virtual ~BT_Chase();

public:
    void OnStart();
    virtual BNode::NODE_STATE Logic_Update(float deltaTime);
    void OnFinish();

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


    static Shared<BT_Chase> Create();
    virtual void Free()override;
};

NS_END
