#pragma once
#include "Task.h"

NS_BEGIN(Client)
class BT_Set_Target :
    public Task
{
    GENERATED_BTNODE(BT_Set_Target, "Task")

public:
    BT_Set_Target();
    virtual ~BT_Set_Target();

public:
    void OnStart();
    virtual BNode::NODE_STATE Logic_Update(float deltaTime);
    void OnFinish();

    static Shared<BT_Set_Target> Create();
    virtual void Free()override;
};

NS_END
