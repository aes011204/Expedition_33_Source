#pragma once
#include "Task.h"

NS_BEGIN(Client)
class BT_Set_TargetAI :
    public Task
{
    GENERATED_BTNODE(BT_Set_TargetAI, "Task")

public:
    BT_Set_TargetAI();
    virtual ~BT_Set_TargetAI();

public:
    void OnStart();
    virtual BNode::NODE_STATE Logic_Update(float deltaTime);
    void OnFinish();

    static Shared<BT_Set_TargetAI> Create();
    virtual void Free()override;
};

NS_END
