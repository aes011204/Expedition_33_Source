#pragma once

#include "SL_Types.h"
#include "Task.h"

NS_BEGIN(Client)

class BT_SL_SetSelectedAttack : public Task
{
    GENERATED_BTNODE(BT_SL_SetSelectedAttack, "Task")

public:
    BT_SL_SetSelectedAttack();
    ~BT_SL_SetSelectedAttack() override = default;

public:
    void OnStart() override;
    BNode::NODE_STATE Logic_Update(float deltaTime) override;
    void OnFinish() override;

private:
    SL_MonAttackType _selectedAttack = SL_MonAttackType::DefaultAttack;
    string _selectedAttackKey = "SelectedAttack";

public:
    static Shared<BT_SL_SetSelectedAttack> Create();
    void Free() override;
};

NS_END
