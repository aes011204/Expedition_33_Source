#pragma once
#include "Task.h"
NS_BEGIN(Client)
class BT_Patroll :
    public Task
{
    GENERATED_BTNODE(BT_Patroll, "Task")

public:
    BT_Patroll();
    virtual ~BT_Patroll();

public:
    void OnStart();
    virtual BNode::NODE_STATE Logic_Update(float deltaTime);
    void OnFinish();

    static Shared<BT_Patroll> Create();
    virtual void Free()override;
};

NS_END
