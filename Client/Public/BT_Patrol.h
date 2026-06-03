#pragma once
#include "Task.h"
NS_BEGIN(Client)
class BT_Patrol :
    public Task
{
    GENERATED_BTNODE(BT_Patrol, "Task")

public:
    BT_Patrol();
    virtual ~BT_Patrol();

public:
    void OnStart();
    virtual BNode::NODE_STATE Logic_Update(float deltaTime);
    void OnFinish();

    static Shared<BT_Patrol> Create();
    virtual void Free()override;

private:
    vector<Vec3> _patrolPoints = {};
    uint32 _curIndex = 0;
    Vec3 _lastSamplePos = Vec3::Zero;
    float _stuckAccum = 0.f;
    bool _usingBypassTarget = false;
    Vec3 _bypassTarget = Vec3::Zero;
    uint32 _bypassAttemptCount = 0;
};

NS_END
