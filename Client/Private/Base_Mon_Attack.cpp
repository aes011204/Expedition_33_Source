#include "pch.h"
#include "Base_Mon_Attack.h"

#include "DamageSystem.h"
#include "ClientInstance.h"

NS_BEGIN(Client)

Base_Mon_Attack::Base_Mon_Attack()
{
}

void Base_Mon_Attack::Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target)
{
    float Damage = 50;
    auto  hero = CLIENT->Get_BattleController()->Get_AliveHeroes();

    if(skill.targetingType == ECharacterTeamSelector::Enemies)
    {
        for(int i =0 ; i < hero.size(); i++)
        {
        DamageSystem::ApplyDamage(Damage, hero[i]);
        }
    }
    else
    {
    DamageSystem::ApplyDamage(Damage, target);
        
    }



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
