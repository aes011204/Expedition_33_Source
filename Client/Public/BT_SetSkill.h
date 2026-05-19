#pragma once
#include "Task.h"

NS_BEGIN(Client)
class BT_SetSkill :
    public Task
{
    GENERATED_BTNODE(BT_SetSkill, "Task")

public:
    BT_SetSkill();
    virtual ~BT_SetSkill();

public:
    void OnStart();
    virtual BNode::NODE_STATE Logic_Update(float deltaTime);
    void OnFinish();

    static Shared<BT_SetSkill> Create();
    virtual void Free()override;
};

NS_END
