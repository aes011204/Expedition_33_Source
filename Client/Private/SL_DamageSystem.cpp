#include "SL_DamageSystem.h"

#include "CombatStatCom.h"
#include "GameInstance.h"

float SL_DamageSystem::Calculate_PhysicalDamage(const Shared<CombatStatCom>& attackerStat, float multiplier)
{
    if (attackerStat == nullptr || multiplier <= 0.f)
        return 0.f;

    const float attack = attackerStat->Get_StatInfo().physicalAttack;

    return max(0.f, attack * multiplier * GAME->Random(0.9f, 1.1f));
}

bool SL_DamageSystem::Apply_Damage(const Shared<GameObject>& attacker, const Shared<CombatStatCom>& attackerStat,
    const Shared<GameObject>& target, const Shared<CombatStatCom>& targetStat, float multiplier, AttackType attackType, SL_DamageInfo& outInfo)
{
    outInfo = {};
    outInfo.attacker = attacker;
    outInfo.target = target;
    outInfo.attackerStat = attackerStat;
    outInfo.targetStat = targetStat;
    outInfo.attackType = attackType;

    if (target == nullptr || targetStat == nullptr)
        return false;

    if (targetStat->Get_AliveStatus() == EAliveStatus::Dead)
        return false;

    float damage = Calculate_PhysicalDamage(attackerStat, multiplier);
    if (damage <= 0.f)
        return false;

    if (attackerStat != nullptr && attackerStat->Is_Player() && attackType != AttackType::BuffEffect)
    {
        const float criticalRate = clamp(attackerStat->Get_StatInfo().criticalRate, 0.f, 100.f);
        outInfo.isCritical = GAME->Random(0.f, 100.f) < criticalRate;
    }

    if (outInfo.isCritical)
        damage *= 1.5f;

    auto& runtimeStat = targetStat->Get_RuntimeStat();
    runtimeStat.CurHp = max(0.f, runtimeStat.CurHp - damage);

    if (runtimeStat.CurHp <= 0.f)
        runtimeStat.CurAliveState = EAliveStatus::Dead;

    outInfo.finalDamage = damage;
    outInfo.isKilled = runtimeStat.CurAliveState == EAliveStatus::Dead;

    return true;
}
