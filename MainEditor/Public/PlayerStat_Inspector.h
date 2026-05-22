#pragma once

#include "CombatStat_Inspector.h"

NS_BEGIN(MainEditor)

class PlayerStat_Inspector : public CombatStat_Inspector
{
public:
    PlayerStat_Inspector() = default;
    ~PlayerStat_Inspector() override = default;

public:
    public:
    void Draw_Inspector(const Shared<Component>& component) override;

public:
    static Shared<PlayerStat_Inspector> Create();
};

NS_END
