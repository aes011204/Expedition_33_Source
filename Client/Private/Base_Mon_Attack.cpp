#include "pch.h"
#include "Base_Mon_Attack.h"

#include "DamageSystem.h"

NS_BEGIN(Client)

Base_Mon_Attack::Base_Mon_Attack()
{
}

void Base_Mon_Attack::Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target)
{
    float Damage = 50;
    DamageSystem::ApplyDamage(Damage, target);

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
