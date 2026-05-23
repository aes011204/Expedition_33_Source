#include "pch.h"
#include "Base_Gustave_Attack.h"

#include "Character.h"
#include "CombatStatCom.h"
#include "DamageSystem.h"

NS_BEGIN(Client)

Base_Gustave_Attack::Base_Gustave_Attack()
{
}

void Base_Gustave_Attack::Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target)
{
    float Damage = 40;
    DamageSystem::ApplyDamage(Damage, target);

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
