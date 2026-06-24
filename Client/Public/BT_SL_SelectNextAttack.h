#pragma once

#include "SL_Types.h"
#include "Task.h"

NS_BEGIN(Client)

class BT_SL_SelectNextAttack final : public Task
{
    GENERATED_BTNODE(BT_SL_SelectNextAttack, "Task")

public:
    BT_SL_SelectNextAttack();
    ~BT_SL_SelectNextAttack() override = default;

public:
    void OnStart() override;
    BNode::NODE_STATE Logic_Update(float deltaTime) override;
    void OnFinish() override;

private:
    string _selectedAttackKey = "SelectedAttack";
    string _attackChainStepKey = "AttackChainStep";

    SL_MonAttackType _attackStep1 = SL_MonAttackType::SingleSlash_1;
    SL_MonAttackType _attackStep2 = SL_MonAttackType::SingleSlash_2;
    SL_MonAttackType _attackStep3 = SL_MonAttackType::SingleSlash_3;
    bool _useStep2 = true;
    bool _useStep3 = true;

public:
    static Shared<BT_SL_SelectNextAttack> Create();
    void Free() override;
};

NS_END
