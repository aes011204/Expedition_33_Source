#include "Skill_Gustave_Battle.h"
#include "BattleMechanismCom.h"
#include "DamageSystem.h"
#include "ClientInstance.h"

/* 뤼미에르 */
void Skill_Gustave_LumiereAssault::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    if (attacker.mechanismCom)
        attacker.mechanismCom->Add_GustaveCharge(1);
}

Shared<Skill_Gustave_LumiereAssault> Skill_Gustave_LumiereAssault::Create()
{
    return make_shared<Skill_Gustave_LumiereAssault>();
}

/* 과충전 */
void Skill_Gustave_UnleashedCharge::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    // 과충전은 추후에, 게이지 비례한 데미지로 변경 예정
    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    battle->Try_ApplySkillBuff(skill, target);

    if (!attacker.mechanismCom)
        return;

    // 과충전 사용 시, 게이지 전부 소모
    attacker.mechanismCom->Consume_AllGustaveCharge();
}

Shared<Skill_Gustave_UnleashedCharge> Skill_Gustave_UnleashedCharge::Create()
{
    return make_shared<Skill_Gustave_UnleashedCharge>();
}

/* 징표 사격 */
void Skill_Gustave_MarkingShot::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();
    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    battle->Apply_Buff(target, Buffs::Mark);

    if (attacker.mechanismCom)
        attacker.mechanismCom->Add_GustaveCharge(1);
}

Shared<Skill_Gustave_MarkingShot> Skill_Gustave_MarkingShot::Create()
{
    return make_shared<Skill_Gustave_MarkingShot>();
}

/* 산산조각 */
void Skill_Gustave_Shatter::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, curTargets, attackType);
}

Shared<Skill_Gustave_Shatter> Skill_Gustave_Shatter::Create()
{
    return make_shared<Skill_Gustave_Shatter>();
}

/* 완벽한 파괴 */
void Skill_Gustave_PerfectBreak::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    if (attacker.mechanismCom)
        attacker.mechanismCom->Add_GustaveCharge(1);
}

Shared<Skill_Gustave_PerfectBreak> Skill_Gustave_PerfectBreak::Create()
{
    return make_shared<Skill_Gustave_PerfectBreak>();
}

/* 천사의 눈 */
void Skill_Gustave_AnglesEye::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    if (attacker.mechanismCom)
        attacker.mechanismCom->Add_GustaveCharge(1);
}

Shared<Skill_Gustave_AnglesEye> Skill_Gustave_AnglesEye::Create()
{
    return make_shared<Skill_Gustave_AnglesEye>();
}
