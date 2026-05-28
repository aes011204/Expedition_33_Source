#pragma once

#include "SkillScript.h"

NS_BEGIN(Client)

class Skill_Lune_Immolation : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Lune_Immolation)

public:
    Skill_Lune_Immolation() = default;
    ~Skill_Lune_Immolation() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target) override;

public:
    static Shared<Skill_Lune_Immolation> Create();
    void Free() override { SkillScript::Free(); }
};

class Skill_Lune_IceGust : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Lune_IceGust)

public:
    Skill_Lune_IceGust() = default;
    ~Skill_Lune_IceGust() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target) override;

public:
    static Shared<Skill_Lune_IceGust> Create();
    void Free() override { SkillScript::Free(); }
};

class Skill_Lune_Thunderfall : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Lune_Thunderfall)

public:
    Skill_Lune_Thunderfall() = default;
    ~Skill_Lune_Thunderfall() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target) override;

public:
    static Shared<Skill_Lune_Thunderfall> Create();
    void Free() override { SkillScript::Free(); }
};

NS_END
