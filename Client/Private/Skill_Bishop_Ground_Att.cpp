#include "Skill_Bishop_Ground_Att.h"

#include "DamageSystem.h"
#include "ClientInstance.h"

Skill_Bishop_Ground_Att::Skill_Bishop_Ground_Att()
{
}

void Skill_Bishop_Ground_Att::Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target)
{
    float Damage = 50;
    auto  hero = CLIENT->Get_BattleController()->Get_AliveHeroes();

    AttackCount++;

    if(AttackCount==1)
    {
        DamageSystem::ApplyDamage(Damage, target);
    }
    else if (AttackCount == skill.AttackCount)
    {
        for (int i = 0; i < hero.size(); i++)
        {
            DamageSystem::ApplyDamage(Damage, hero[i]);
        }
    }
    else
    {
        int num = Math::RandNum(0, hero.size());

        DamageSystem::ApplyDamage(Damage, hero[num]);

    }



}

Shared<Skill_Bishop_Ground_Att> Skill_Bishop_Ground_Att::Create()
{
    return make_shared<Skill_Bishop_Ground_Att>();
}

void Skill_Bishop_Ground_Att::Free()
{
    SkillScript::Free();
}
