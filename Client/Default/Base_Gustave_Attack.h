
#pragma once
#include "SkillScript.h"

NS_BEGIN(Client)

class Base_Gustave_Attack : public SkillScript
{
    GENERATED_SKILLSCRIPT(Base_Gustave_Attack)

public:
    Base_Gustave_Attack();
    virtual ~Base_Gustave_Attack() = default;

public:
    virtual void Execute(const Skill_Data& skill,
        const BattleAction& action,
        AttackType attackType) override;

public:
    static Shared<Base_Gustave_Attack> Create();
    void Free() override;

private:
    uint32 CurIndex = {};
};

NS_END
