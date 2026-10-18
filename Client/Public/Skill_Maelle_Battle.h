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
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

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
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

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
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Skill_Maelle_FireRise> Create();
    void Free() override { SkillScript::Free(); }
};

class Skill_Maelle_Spark : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Maelle_Spark)

public:
    Skill_Maelle_Spark() = default;
    ~Skill_Maelle_Spark() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Skill_Maelle_Spark> Create();
    void Free() override { SkillScript::Free(); }
};

class Skill_Maelle_Stendhal : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Maelle_Stendhal)

public:
    Skill_Maelle_Stendhal() = default;
    ~Skill_Maelle_Stendhal() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Skill_Maelle_Stendhal> Create();
    void Free() override { SkillScript::Free(); }
};

class Skill_Maelle_SwordBallet : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Maelle_SwordBallet)

public:
    Skill_Maelle_SwordBallet() = default;
    ~Skill_Maelle_SwordBallet() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Skill_Maelle_SwordBallet> Create();
    void Free() override { SkillScript::Free(); }
};

class Skill_Maelle_FleureFury : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Maelle_FleureFury)

public:
    Skill_Maelle_FleureFury() = default;
    ~Skill_Maelle_FleureFury() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Skill_Maelle_FleureFury> Create();
    void Free() override { SkillScript::Free(); }
};

class Skill_Maelle_Pyrolyse : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Maelle_Pyrolyse)

public:
    Skill_Maelle_Pyrolyse() = default;
    ~Skill_Maelle_Pyrolyse() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Skill_Maelle_Pyrolyse> Create();
    void Free() override { SkillScript::Free(); }
};

NS_END
