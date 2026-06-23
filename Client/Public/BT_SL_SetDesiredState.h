#pragma once

#include "SL_Types.h"
#include "Task.h"

NS_BEGIN(Client)

class BT_SL_SetDesiredState : public Task
{
    GENERATED_BTNODE(BT_SL_SetDesiredState, "Task")

public:
    BT_SL_SetDesiredState();
    ~BT_SL_SetDesiredState() override = default;

public:
    void OnStart() override;
    BNode::NODE_STATE Logic_Update(float deltaTime) override;
    void OnFinish() override;

private:
    SL_MonState _desiredState = SL_MonState::Idle;
    string _desiredStateKey = "DesiredState";

public:
    static Shared<BT_SL_SetDesiredState> Create();
    void Free() override;
};

NS_END
