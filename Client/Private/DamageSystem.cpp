#include "pch.h"
#include "DamageSystem.h"

#include "Battle_Enum.h"
#include "CombatStatCom.h"

FOnDamageApplied DamageSystem::OnDamageApplied{};
FOnHealApplied DamageSystem::OnHealApplied{};

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

    if(target.statCom->Get_CurrentShield()!=0)
    {
        target.statCom->Remove_Shield(1);
        return;
    }

    if (target.statCom->Get_AliveStatus() == EAliveStatus::Dead)
        return;

    auto& runtimeStat = target.statCom->Get_RuntimeStat();
    LOG_DEBUG("ApplyDamage: before hp={:.2f}, damage={:.2f}", runtimeStat.CurHp, damage);
    runtimeStat.CurHp -= damage;

    if (runtimeStat.CurHp <= 0.f)
    {
        runtimeStat.CurHp = 0.f;
        runtimeStat.CurAliveState = EAliveStatus::Dead;
    }

    LOG_DEBUG("ApplyDamage: after hp={:.2f}, alive={}",
        runtimeStat.CurHp,
        static_cast<int>(runtimeStat.CurAliveState));

    OnDamageApplied.Broadcast(target, damage,attackType);
}

void DamageSystem::ApplyDamage(float damage, const vector<BattleChar>& target, AttackType attackType)
{
    for(auto character : target)
    {
        ApplyDamage(damage, character, attackType);
    }
}

void DamageSystem::Free()
{
    Base::Free();
}
