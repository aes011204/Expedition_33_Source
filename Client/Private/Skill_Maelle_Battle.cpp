#include "Skill_Maelle_Battle.h"
#include "BattleMechanismCom.h"
#include "DamageSystem.h"
#include "ClientInstance.h"

/* 공격 태세 */
void Skill_Maelle_OffensiveSwitch::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Offensive);
}

Shared<Skill_Maelle_OffensiveSwitch> Skill_Maelle_OffensiveSwitch::Create()
{
    return make_shared<Skill_Maelle_OffensiveSwitch>();
}

/* 돌파 */
void Skill_Maelle_Percee::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Defensive);
}

Shared<Skill_Maelle_Percee> Skill_Maelle_Percee::Create()
{
    return make_shared<Skill_Maelle_Percee>();
}

/* 빠른 걸음 */
void Skill_Maelle_FireRise::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);
    
    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Virtuose);
}

Shared<Skill_Maelle_FireRise> Skill_Maelle_FireRise::Create()
{
    return make_shared<Skill_Maelle_FireRise>();
}

/* 플뢰레 격노 */
void Skill_Maelle_FleureFury::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    battle->Try_ApplySkillBuff(skill, target);
    
    // 임시로 명인태세
    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Virtuose);
}

Shared<Skill_Maelle_FleureFury> Skill_Maelle_FleureFury::Create()
{
    return make_shared<Skill_Maelle_FleureFury>();
}

/* 불꽃 */
void Skill_Maelle_Spark::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);
    
    // 임시로 명인태세
    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Virtuose);

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    battle->Try_ApplySkillBuff(skill, target);
}

Shared<Skill_Maelle_Spark> Skill_Maelle_Spark::Create()
{
    return make_shared<Skill_Maelle_Spark>();
}

/* 스탕달 */
void Skill_Maelle_Stendhal::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);
    
    // 임시로 명인태세
    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Virtuose);
}

Shared<Skill_Maelle_Stendhal> Skill_Maelle_Stendhal::Create()
{
    return make_shared<Skill_Maelle_Stendhal>();
}

/* 검의 발레 */
void Skill_Maelle_SwordBallet::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    // 임시로 명인태세
    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Virtuose);
}

Shared<Skill_Maelle_SwordBallet> Skill_Maelle_SwordBallet::Create()
{
    return make_shared<Skill_Maelle_SwordBallet>();
}

/* 열분해 */
void Skill_Maelle_Pyrolyse::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);
    
    // 임시로 명인태세
    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Virtuose);

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    battle->Try_ApplySkillBuff(skill, target);
}

Shared<Skill_Maelle_Pyrolyse> Skill_Maelle_Pyrolyse::Create()
{
    return make_shared<Skill_Maelle_Pyrolyse>();
}
