#pragma once
#include "CombatStat_Inspector.h"

NS_BEGIN(MainEditor)

class MonsterStat_Inspector final : public CombatStat_Inspector
{
public:
    MonsterStat_Inspector() = default;
    ~MonsterStat_Inspector() override = default;

public:
    void Draw_Inspector(const Shared<Component>& component) override;

public:
    static Shared<MonsterStat_Inspector> Create();
};

NS_END
