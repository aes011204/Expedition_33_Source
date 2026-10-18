#pragma once

#include "Task.h"

NS_BEGIN(Client)

class BT_ForceSkillOnDeadHero : public Task
{
    GENERATED_BTNODE(BT_ForceSkillOnDeadHero, "Task")

public:
    BT_ForceSkillOnDeadHero();
    ~BT_ForceSkillOnDeadHero() override = default;

public:
    void OnStart() override;
    BNode::NODE_STATE Logic_Update(float deltaTime) override;
    void OnFinish() override;

private:
    string _forcedSkillKey = "";

public:
    static Shared<BT_ForceSkillOnDeadHero> Create();
    void Free() override;
};

NS_END
