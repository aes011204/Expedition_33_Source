#pragma once
#include "SkillScript.h"

NS_BEGIN(Client)
class Base_Mon_Spawn :
    public SkillScript
{
    GENERATED_SKILLSCRIPT(Base_Mon_Spawn)

public:
    Base_Mon_Spawn();
    virtual ~Base_Mon_Spawn() = default;
public:
    virtual void Execute(const Skill_Data& skill,
        const BattleChar& attacker,
        const BattleChar& target);
public:
    static Shared<Base_Mon_Spawn> Create();
    //Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
