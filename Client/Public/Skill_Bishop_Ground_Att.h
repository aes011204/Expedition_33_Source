#pragma once
#include "SkillScript.h"

NS_BEGIN(Client)
class Skill_Bishop_Ground_Att :
    public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Bishop_Ground_Att)

public:
    Skill_Bishop_Ground_Att();
    virtual ~Skill_Bishop_Ground_Att() = default;
public:
    virtual void Execute(const Skill_Data& skill,
        const BattleAction& action, AttackType attackType) override;


private:
    int32 AttackCount = { 0 };


public:
    static Shared<Skill_Bishop_Ground_Att> Create();
    //Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
