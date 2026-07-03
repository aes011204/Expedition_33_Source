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
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;
    void Collect_PrewarmEffects(const Skill_Data& skill, const BattleEffectPrewarmContext& context, vector<EffectPrewarmRequest>& outRequests) const override;

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
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

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
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;
    void Collect_PrewarmEffects(const Skill_Data& skill, const BattleEffectPrewarmContext& context, vector<EffectPrewarmRequest>& outRequests) const override;

public:
    static Shared<Skill_Lune_Thunderfall> Create();
    void Free() override { SkillScript::Free(); }
};

class Skill_Lune_TerraQuake : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Lune_TerraQuake)

public:
    Skill_Lune_TerraQuake() = default;
    ~Skill_Lune_TerraQuake() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;
    void Collect_PrewarmEffects(const Skill_Data& skill, const BattleEffectPrewarmContext& context, vector<EffectPrewarmRequest>& outRequests) const override;

public:
    static Shared<Skill_Lune_TerraQuake> Create();
    void Free() override { SkillScript::Free(); }
};

class Skill_Lune_CripplingTsunami : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Lune_CripplingTsunami)

public:
    Skill_Lune_CripplingTsunami() = default;
    ~Skill_Lune_CripplingTsunami() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Skill_Lune_CripplingTsunami> Create();
    void Free() override { SkillScript::Free(); }
};

class Skill_Lune_SkyBreak : public SkillScript
{
    GENERATED_SKILLSCRIPT(Skill_Lune_SkyBreak)

public:
    Skill_Lune_SkyBreak() = default;
    ~Skill_Lune_SkyBreak() override = default;

public:
    void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Skill_Lune_SkyBreak> Create();
    void Free() override { SkillScript::Free(); }
};

NS_END
