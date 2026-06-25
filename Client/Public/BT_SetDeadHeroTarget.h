#pragma once

#include "Task.h"

NS_BEGIN(Client)

class BT_SetDeadHeroTarget : public Task
{
    GENERATED_BTNODE(BT_SetDeadHeroTarget, "Task")

public:
    BT_SetDeadHeroTarget();
    ~BT_SetDeadHeroTarget() override = default;

public:
    void OnStart() override;
    BNode::NODE_STATE Logic_Update(float deltaTime) override;
    void OnFinish() override;

public:
    static Shared<BT_SetDeadHeroTarget> Create();
    void Free() override;
};

NS_END
