
#pragma once
#include "SkillScript.h"

NS_BEGIN(Client)
class Simon_Debuff :
    public SkillScript
{
    GENERATED_SKILLSCRIPT(Simon_Debuff)

public:
    Simon_Debuff();
    virtual ~Simon_Debuff() = default;
public:
    virtual void Execute(const Skill_Data& skill,
        const BattleAction& action, AttackType attackType) override;
public:
    static Shared<Simon_Debuff> Create();
    //Shared<Component> Clone(void* arg) override;
    void Free() override;


private:
    uint32 CurIndex = {};
};

NS_END
