#pragma once
#include "Task.h"
NS_BEGIN(Client)
class BT_Idle :
    public Task
{
public:
    BT_Idle();
    virtual ~BT_Idle();

public:
    void OnStart();
    virtual BNode::NODE_STATE Logic_Update(float deltaTime);
    void OnFinish();

    static Shared<BT_Idle> Create();
   virtual void Free()override;
};

NS_END
