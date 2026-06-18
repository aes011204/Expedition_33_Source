#include "pch.h"
#include "DamageSystem.h"
#include "GameInstance.h"
#include "Battle_Enum.h"
#include "CombatStatCom.h"

#include "BattleController.h"
#include "ClientInstance.h"
#include "PictosBattleSystem.h"

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
        const uint32 prevShield = target.statCom->Get_CurrentShield();
        target.statCom->Remove_Shield(1);

        DamageAppliedInfo info{};
        info.attacker = attacker;
        info.target = target;
        info.damage = 0.f;
        info.attackType = attackType;
        info.isShieldBlocked = true;
        info.prevShield = prevShield;
        info.currentShield = target.statCom->Get_CurrentShield();

        OnDamageApplied.Broadcast(info);
        return; // 쉴드가 데미지를 막은 경우
    }

    if (target.statCom->Get_AliveStatus() == EAliveStatus::Dead)
        return;

    if (const auto battleController = CLIENT->Get_BattleController())
    {
        if (const auto pictosSystem = battleController->Get_PictosBattleSystem())
        {
            damage *= pictosSystem->Resolve_DamageMultiplier(attacker, target, attackType);
        }
    }

    if (damage <= 0.f)
        return;

    OnDamageCalculating.Broadcast(attacker, target, damage, attackType);

    if (damage <= 0.f)
        return;

    bool isCritical = false;
    if (attacker.statCom != nullptr &&
        attacker.statCom->Is_Player() &&
        attackType != AttackType::BuffEffect &&
        attackType != AttackType::Counter)
    {
        // 크리티컬 확률 구하기
        const float criticalRate = clamp(attacker.statCom->Get_StatInfo().criticalRate, 0.f, 100.f);
        // 크리티컬 성공 여부
        isCritical = GAME->Random(0.f, 100.f) < criticalRate;
    }

    if (isCritical)
        damage *= 1.5f;

    auto& runtimeStat = target.statCom->Get_RuntimeStat();
    runtimeStat.CurHp -= damage;

    if (runtimeStat.CurHp <= 0.f)
    {
        runtimeStat.CurHp = 0.f;
        runtimeStat.CurAliveState = EAliveStatus::Dead;
    }

    DamageAppliedInfo info{};
    info.attacker = attacker;
    info.target = target;
    info.damage = damage;
    info.attackType = attackType;
    info.isCritical = isCritical;

    OnDamageApplied.Broadcast(info);
}

void DamageSystem::ApplyDamage(float damage, const BattleChar& attacker, const vector<BattleChar>& target,
    AttackType attackType)
{
    for (const auto& character : target)
    {
        ApplyDamage(damage, attacker, character, attackType);
    }
}

float DamageSystem::CalculatePlayerAttackDamage(const BattleChar& attacker, float multiplier)
{
    if (attacker.statCom == nullptr || multiplier <= 0.f)
        return 0.f;

    const float attack = attacker.statCom->Get_StatInfo().physicalAttack;
    const float baseDamage = max(0.f, attack * multiplier);

    // 대충 랜덤값 추가
    const float variance = GAME->Random(0.9f, 1.1f);

    return baseDamage * variance;
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
