#pragma once
#include "Base.h"
#include "BattleController.h"
NS_BEGIN(Client)
class SkillScript :
    public Base
{
public:
    SkillScript();
    //SkillScript(const SkillScript& Prototype);
    virtual ~SkillScript() = default;
public:
    virtual void Execute(const Skill_Data& skill,
        const BattleChar& attacker,
        const BattleChar& target) = 0;
public:
    //static Shared<SkillScript> Create();
    //Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
