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
  const BattleAction& action, AttackType attackType) = 0;

    using SkillScriptCreateFn = function<Shared<SkillScript>()>;

    static bool Register_SkillScriptType(const string& className, SkillScriptCreateFn createFn);
    static Shared<SkillScript> Create_SkillScriptByClass(const string& className);

public:
    //static Shared<SkillScript> Create();
    //Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
