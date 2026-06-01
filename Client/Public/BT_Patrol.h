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
};

NS_END
