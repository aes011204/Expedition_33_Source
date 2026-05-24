#pragma once
#include "SkillScript.h"

NS_BEGIN(Client)
class Base_Mon_Defense :
    public SkillScript
{
    GENERATED_SKILLSCRIPT(Base_Mon_Defense)

public:
    Base_Mon_Defense();
    virtual ~Base_Mon_Defense() = default;
public:
    virtual void Execute(const Skill_Data& skill,
        const BattleChar& attacker,
        const BattleChar& target);
public:
    static Shared<Base_Mon_Defense> Create();
    //Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
