#pragma once
#include "SkillScript.h"


NS_BEGIN(Client)
class Base_Attack :
    public SkillScript
{
public:
    Base_Attack();
    virtual ~Base_Attack() = default;
public:
    virtual void Execute(const Skill_Data& skill,
        const BattleChar& attacker,
        const BattleChar& target);
public:
    static Shared<Base_Attack> Create();
    //Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
