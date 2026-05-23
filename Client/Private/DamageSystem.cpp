#include "pch.h"
#include "DamageSystem.h"

#include "Battle_Enum.h"
#include "CombatStatCom.h"

FOnDamageApplied DamageSystem::OnDamageApplied{};

void DamageSystem::Heal(float amount, const BattleChar& target)
{
    auto& runtimeStat = target.statCom->Get_RuntimeStat();
        runtimeStat.CurHp += amount;

    if (runtimeStat.CurHp > target.statCom->Get_StatInfo().maxHp)
    {
        runtimeStat.CurHp = target.statCom->Get_StatInfo().maxHp;
    }
}

void DamageSystem::ApplyDamage(float damage, const BattleChar& target)
{
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

    OnDamageApplied.Broadcast(target, damage);
}

void DamageSystem::Free()
{
    Base::Free();
}
