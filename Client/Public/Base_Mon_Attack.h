#pragma once
#include "SkillScript.h"

NS_BEGIN(Client)
class Base_Mon_Attack :
    public SkillScript
{
    GENERATED_SKILLSCRIPT(Base_Mon_Attack)

public:
    Base_Mon_Attack();
    virtual ~Base_Mon_Attack() = default;
public:
    virtual void Execute(const Skill_Data& skill,
        const BattleChar& attacker,
        const BattleChar& target);
public:
    static Shared<Base_Mon_Attack> Create();
    //Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
