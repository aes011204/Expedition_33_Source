#include "Skill_Maelle_Battle.h"
#include "BattleMechanismCom.h"
#include "DamageSystem.h"

/* 공격 태세 */
void Skill_Maelle_OffensiveSwitch::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    DamageSystem::ApplyDamage(15.f, attacker, target, attackType);

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

    DamageSystem::ApplyDamage(40.f, attacker, target, attackType);

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

    DamageSystem::ApplyDamage(13.f, attacker, target, attackType);

    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Virtuose);
}

Shared<Skill_Maelle_FireRise> Skill_Maelle_FireRise::Create()
{
    return make_shared<Skill_Maelle_FireRise>();
}

void Skill_Maelle_FleureFury::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    DamageSystem::ApplyDamage(13.f, attacker, target, attackType);

    // 임시로 명인태세
    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Virtuose);
}

Shared<Skill_Maelle_FleureFury> Skill_Maelle_FleureFury::Create()
{
    return make_shared<Skill_Maelle_FleureFury>();
}
