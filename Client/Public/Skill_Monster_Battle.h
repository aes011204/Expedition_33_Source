#pragma once

#include "SkillScript.h"

NS_BEGIN(Client)

class Base_Mon_Attack : public SkillScript
{
    GENERATED_SKILLSCRIPT(Base_Mon_Attack)

public:
    Base_Mon_Attack();
    virtual ~Base_Mon_Attack() = default;

public:
    virtual void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Base_Mon_Attack> Create();
    void Free() override;

private:
    uint32 CurIndex = {};
};

class Base_Mon_Attack_Projectile : public SkillScript
{
    GENERATED_SKILLSCRIPT(Base_Mon_Attack_Projectile)

public:
    Base_Mon_Attack_Projectile();
    virtual ~Base_Mon_Attack_Projectile() = default;

public:
    virtual void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Base_Mon_Attack_Projectile> Create();
    void Free() override;

private:
    uint32 CurIndex = {};
};

class Potato_Fire_Skill : public SkillScript
{
    GENERATED_SKILLSCRIPT(Potato_Fire_Skill)

public:
    Potato_Fire_Skill();
    virtual ~Potato_Fire_Skill() = default;

public:
    virtual void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Potato_Fire_Skill> Create();
    void Free() override;

private:
    uint32 CurIndex = {};
};

class Potato_Thunder_Skill : public SkillScript
{
    GENERATED_SKILLSCRIPT(Potato_Thunder_Skill)

public:
    Potato_Thunder_Skill();
    virtual ~Potato_Thunder_Skill() = default;

public:
    virtual void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Potato_Thunder_Skill> Create();
    void Free() override;

private:
    uint32 CurIndex = {};
};

class Bishop_Spear : public SkillScript
{
    GENERATED_SKILLSCRIPT(Bishop_Spear)

public:
    Bishop_Spear();
    virtual ~Bishop_Spear() = default;

public:
    virtual void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Bishop_Spear> Create();
    void Free() override;

private:
    uint32 CurIndex = {};
};

class Base_Mon_Defense : public SkillScript
{
    GENERATED_SKILLSCRIPT(Base_Mon_Defense)

public:
    Base_Mon_Defense();
    virtual ~Base_Mon_Defense() = default;

public:
    virtual void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Base_Mon_Defense> Create();
    void Free() override;
};

class Base_Mon_Spawn : public SkillScript
{
    GENERATED_SKILLSCRIPT(Base_Mon_Spawn)

public:
    Base_Mon_Spawn();
    virtual ~Base_Mon_Spawn() = default;

public:
    virtual void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Base_Mon_Spawn> Create();
    void Free() override;
};

class Simon_Combo : public SkillScript
{
    GENERATED_SKILLSCRIPT(Simon_Combo)

public:
    Simon_Combo();
    virtual ~Simon_Combo() = default;

public:
    virtual void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Simon_Combo> Create();
    void Free() override;

private:
    uint32 CurIndex = {};
};

class Simon_Debuff : public SkillScript
{
    GENERATED_SKILLSCRIPT(Simon_Debuff)

public:
    Simon_Debuff();
    virtual ~Simon_Debuff() = default;

public:
    virtual void Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType) override;

public:
    static Shared<Simon_Debuff> Create();
    void Free() override;

private:
    uint32 CurIndex = {};
};

NS_END
