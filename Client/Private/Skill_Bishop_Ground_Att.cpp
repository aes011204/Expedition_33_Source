#include "Skill_Bishop_Ground_Att.h"

#include "DamageSystem.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"

Skill_Bishop_Ground_Att::Skill_Bishop_Ground_Att()
{
}

void Skill_Bishop_Ground_Att::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    UNREFERENCED_PARAMETER(skill);
    UNREFERENCED_PARAMETER(action);
    UNREFERENCED_PARAMETER(attackType);

    const float Damage = 50;
    const auto hero = CLIENT->Get_BattleController()->Get_AliveHeroes();
    if (hero.empty())
    {
        return;
    }

    AttackCount++;

    DamageSystem::ApplyDamage(Damage, action.GetCurStepTarget(), attackType);

   /* if (AttackCount == 1)
        DamageSystem::ApplyDamage(Damage, target);
    else if (AttackCount == skill.AttackCount)
    {
        for (int i = 0; i < hero.size(); i++)
        {
            DamageSystem::ApplyDamage(Damage, hero[i],);
        }
        return;
    }
    else
    {
        const int num = Math::RandNum(0, hero.size() - 1);

            DamageSystem::ApplyDamage(Damage, hero[num]);
    }*/
}

Shared<Skill_Bishop_Ground_Att> Skill_Bishop_Ground_Att::Create()
{
    return make_shared<Skill_Bishop_Ground_Att>();
}

void Skill_Bishop_Ground_Att::Free()
{
    SkillScript::Free();
}
