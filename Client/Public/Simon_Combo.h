#pragma once
#include "SkillScript.h"

NS_BEGIN(Client)
class Simon_Combo :
    public SkillScript
{
    GENERATED_SKILLSCRIPT(Simon_Combo)

public:
    Simon_Combo();
    virtual ~Simon_Combo() = default;
public:
    virtual void Execute(const Skill_Data& skill,
        const BattleAction& action, AttackType attackType) override;
public:
    static Shared<Simon_Combo> Create();
    //Shared<Component> Clone(void* arg) override;
    void Free() override;


private:
    uint32 CurIndex = {};
};

NS_END
