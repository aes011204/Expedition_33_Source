#include "pch.h"
#include "Simon_Debuff.h"

#include "DamageSystem.h"

#include "ClientInstance.h"

NS_BEGIN(Client)

Simon_Debuff::Simon_Debuff()
{
}

void Simon_Debuff::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    UNREFERENCED_PARAMETER(attackType);

    //const float Damage = 50;

    if (CurIndex >= skill.targetingType.size())
    {
        LOG_ERROR("ERROR_Simon_Debuff_ OutofRange");
        return;
    }
    const BattleChar& attacker = action.Get_CurChar();
    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);

    DamageSystem::ApplyDamage(damage, action.GetCurStepTarget(), attackType);

    /* if (skill.targetingType[CurIndex] == TargetType::AllEnemies)
     {
         for (int i = 0; i < hero.size(); i++)
             DamageSystem::ApplyDamage(Damage, hero[i],AttackType::SingleAOE);
     }
     else if (skill.targetingType[CurIndex] == TargetType::Primary)
         DamageSystem::ApplyDamage(Damage, action.target,AttackType::SingleSkill);
     else if (skill.targetingType[CurIndex] == TargetType::RandomEnemy)
     {

         const int num = Math::RandNum(0, hero.size() - 1);
         DamageSystem::ApplyDamage(Damage, hero[num],AttackType::Combo);

     }*/

    CurIndex++;

}

Shared<Simon_Debuff> Simon_Debuff::Create()
{
    return make_shared<Simon_Debuff>();
}

void Simon_Debuff::Free()
{
    SkillScript::Free();
}

NS_END
