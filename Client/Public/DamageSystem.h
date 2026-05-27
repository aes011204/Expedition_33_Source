#pragma once

#include "Base.h"
#include "Delegate.h"

NS_BEGIN(Client)

enum class AttackType;
struct BattleChar;

DECLARE_DELEGATE(FOnDamageApplied, const BattleChar&, float, AttackType);
DECLARE_DELEGATE(FOnHealApplied, const BattleChar&, float);

class DamageSystem : public Base
{
public:
    DamageSystem() = default;
    virtual ~DamageSystem() = default;
public:
    static void ApplyDamage(float damage, const BattleChar& target, AttackType attackType);
    static void Heal(float amount, const BattleChar& target);

    static FOnDamageApplied OnDamageApplied;
    static FOnHealApplied   OnHealApplied;

    virtual void Free();
};


NS_END
