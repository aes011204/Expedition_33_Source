#pragma once

#include "SkillScript.h"

NS_BEGIN(Client)

class Skill_Maelle_OffensiveSwitch : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Maelle_OffensiveSwitch)

public:
    Skill_Maelle_OffensiveSwitch() = default;
    ~Skill_Maelle_OffensiveSwitch() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target) override;

public:
    static Shared<Skill_Maelle_OffensiveSwitch> Create();
    void Free() override { SkillScript::Free(); }
};

class Skill_Maelle_Percee : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Maelle_Percee)

public:
    Skill_Maelle_Percee() = default;
    ~Skill_Maelle_Percee() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target) override;

public:
    static Shared<Skill_Maelle_Percee> Create();
    void Free() override { SkillScript::Free(); }
};

class Skill_Maelle_FireRise : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Maelle_FireRise)

public:
    Skill_Maelle_FireRise() = default;
    ~Skill_Maelle_FireRise() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target) override;

public:
    static Shared<Skill_Maelle_FireRise> Create();
    void Free() override { SkillScript::Free(); }
};

NS_END
