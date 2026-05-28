#pragma once

#include "SkillScript.h"

NS_BEGIN(Client)

class Skill_Gustave_LumiereAssault : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Gustave_LumiereAssault)

public:
    Skill_Gustave_LumiereAssault() = default;
    ~Skill_Gustave_LumiereAssault() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Skill_Gustave_LumiereAssault> Create();
    void Free() override { SkillScript::Free(); }
};

class Skill_Gustave_UnleashedCharge : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Gustave_UnleashedCharge)

public:
    Skill_Gustave_UnleashedCharge() = default;
    ~Skill_Gustave_UnleashedCharge() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Skill_Gustave_UnleashedCharge> Create();
    void Free() override { SkillScript::Free(); }
};

class Skill_Gustave_MarkingShot : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Gustave_MarkingShot)

public:
    Skill_Gustave_MarkingShot() = default;
    ~Skill_Gustave_MarkingShot() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Skill_Gustave_MarkingShot> Create();
    void Free() override { SkillScript::Free(); }
};

NS_END
