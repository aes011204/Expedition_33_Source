#include "pch.h"
#include "DamageSystem.h"

#include "Battle_Enum.h"
#include "CombatStatCom.h"

FOnDamageApplied        DamageSystem::OnDamageApplied{};
FOnDamageCalculating    DamageSystem::OnDamageCalculating{};
FOnHealApplied          DamageSystem::OnHealApplied{};

void DamageSystem::Heal(float amount, const BattleChar& target)
{
    if (amount <= 0.f || target.statCom == nullptr)
        return;

    auto& runtimeStat = target.statCom->Get_RuntimeStat();
    const float beforeHp = runtimeStat.CurHp;
    const float maxHp = target.statCom->Get_StatInfo().maxHp;

    runtimeStat.CurHp += amount;

    if (runtimeStat.CurHp > maxHp)
        runtimeStat.CurHp = maxHp;

    const float healedAmount = runtimeStat.CurHp - beforeHp;
    if (healedAmount <= 0.f)
        return;

    OnHealApplied.Broadcast(target, healedAmount);
}

void DamageSystem::ApplyDamage(float damage, const BattleChar& target, AttackType attackType)
{
    BattleChar emptyAttacker{};
    ApplyDamage(damage, emptyAttacker, target, attackType);
}

void DamageSystem::ApplyDamage(float damage, const BattleChar& attacker, const BattleChar& target,
    AttackType attackType)
{
    if (damage <= 0.f || target.statCom == nullptr)
        return;

    if (target.statCom->Get_CurrentShield() != 0)
    {
        target.statCom->Remove_Shield(1);
        return; // 일단 쉴드는 징표 소비 X
    }

    OnDamageCalculating.Broadcast(attacker, target, damage, attackType);

    // 데미지 적용 후 다시 판단
    if (damage <= 0.f)
        return;

    auto& runtimeStat = target.statCom->Get_RuntimeStat();
    //LOG_DEBUG("ApplyDamage: before hp={:.2f}, damage={:.2f}", runtimeStat.CurHp, damage);
    runtimeStat.CurHp -= damage;

    if (runtimeStat.CurHp <= 0.f)
    {
        runtimeStat.CurHp = 0.f;
        runtimeStat.CurAliveState = EAliveStatus::Dead;
    }

    //LOG_DEBUG("ApplyDamage: after hp={:.2f}, alive={}",
    //    runtimeStat.CurHp,
    //    static_cast<int>(runtimeStat.CurAliveState));

    OnDamageApplied.Broadcast(target, damage, attackType);
}

void DamageSystem::ApplyDamage(float damage, const BattleChar& attacker, const vector<BattleChar>& target,
    AttackType attackType)
{
    for (const auto& character : target)
    {
        ApplyDamage(damage, attacker, character, attackType);
    }
}

void DamageSystem::ApplyDamage(float damage, const vector<BattleChar>& target, AttackType attackType)
{
    BattleChar emptyAttacker{};

    for (const auto& character : target)
    {
        ApplyDamage(damage, emptyAttacker, character, attackType);
    }
}

void DamageSystem::Free()
{
    Base::Free();
}
