#pragma once
#include "Task.h"

NS_BEGIN(Client)
class BT_FinishDecision :
    public Task
{
    GENERATED_BTNODE(BT_FinishDecision, "Task")

public:
    BT_FinishDecision();
    virtual ~BT_FinishDecision();

public:
    void OnStart();
    virtual BNode::NODE_STATE Logic_Update(float deltaTime);
    void OnFinish();

    static Shared<BT_FinishDecision> Create();
    virtual void Free()override;
};

NS_END
