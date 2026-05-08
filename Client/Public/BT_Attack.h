#pragma once
#include "Task.h"

NS_BEGIN(Client)
class BT_Attack :
    public Task
{
    GENERATED_BTNODE(BT_Attack, "Task")
public:
    BT_Attack();
    virtual ~BT_Attack();

public:
    void OnStart();
    virtual BNode::NODE_STATE Logic_Update(float deltaTime);
    void OnFinish();

    static Shared<BT_Attack> Create();
    virtual void Free()override;
};

NS_END
