#pragma once
#include "Task.h"
NS_BEGIN(Client)

class Monster_WorldBase;

class BT_Stun :
    public Task
{
    GENERATED_BTNODE(BT_Stun, "Task")

public:
    BT_Stun();
    virtual ~BT_Stun();

public:
    void OnStart();
    virtual BNode::NODE_STATE Logic_Update(float deltaTime);
    void OnFinish();

private:

  //  Shared<Monster_WorldBase> monster = {};

    static Shared<BT_Stun> Create();
    virtual void Free()override;
};

NS_END
