#pragma once
#include <BlackBoard.h>
#include "Task.h"

NS_BEGIN(Client)
class BT_SetSkillAI :
    public Task
{
    GENERATED_BTNODE(BT_SetSkillAI, "Task")

public:
    BT_SetSkillAI();
    virtual ~BT_SetSkillAI();

public:
    void OnStart();
    virtual BNode::NODE_STATE Logic_Update(float deltaTime);
    void SelectSkill(const Shared<BlackBoard>& bb, int32 SkillIndex);
    void OnFinish();

    static Shared<BT_SetSkillAI> Create();
    virtual void Free()override;
};

NS_END
