#include "pch.h"
#include "Base_Mon_Attack.h"

#include "DamageSystem.h"

#include "ClientInstance.h"

NS_BEGIN(Client)

Base_Mon_Attack::Base_Mon_Attack()
{
}

void Base_Mon_Attack::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    UNREFERENCED_PARAMETER(attackType);

    //const float Damage = 50;

    if(CurIndex >= skill.targetingType.size())
    {
        LOG_ERROR("ERROR_Base_Mon_Attack_ OutofRange");
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

Shared<Base_Mon_Attack> Base_Mon_Attack::Create()
{
    return make_shared<Base_Mon_Attack>();
}

void Base_Mon_Attack::Free()
{
    SkillScript::Free();
}

NS_END
