#include "pch.h"
#include "Base_Gustave_Attack.h"

#include "Character.h"
#include "CombatStatCom.h"
#include "ClientInstance.h"
#include "DamageSystem.h"

NS_BEGIN(Client)

Base_Gustave_Attack::Base_Gustave_Attack()
{
}

void Base_Gustave_Attack::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    float Damage = 40;

    const auto enemy = CLIENT->Get_BattleController()->Get_AliveEnemies();

    if (CurIndex >= skill.targetingType.size())
    {
        LOG_ERROR("ERROR_Base_Mon_Attack_ OutofRange");
        return;
    }

    DamageSystem::ApplyDamage(Damage, action.GetCurStepTarget(), attackType);


    /*if (skill.targetingType[CurIndex] == TargetType::AllEnemies)
    {
        for (int i = 0; i < enemy.size(); i++)
            DamageSystem::ApplyDamage(Damage, enemy[i], AttackType::SingleAOE);
    }
    else if (skill.targetingType[CurIndex] == TargetType::Primary)
        DamageSystem::ApplyDamage(Damage, action.target, AttackType::SingleSkill);
    else if (skill.targetingType[CurIndex] == TargetType::RandomEnemy)
    {

        const int num = Math::RandNum(0, enemy.size() - 1);
        DamageSystem::ApplyDamage(Damage, enemy[num], AttackType::Combo);
    }*/

    CurIndex++;

}

Shared<Base_Gustave_Attack> Base_Gustave_Attack::Create()
{
    return make_shared<Base_Gustave_Attack>();
}

void Base_Gustave_Attack::Free()
{
    SkillScript::Free();
}

NS_END
