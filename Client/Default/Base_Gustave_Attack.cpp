#include "pch.h"
#include "Base_Gustave_Attack.h"

#include "BattleMechanismCom.h"
#include "Character.h"
#include "CombatStatCom.h"
#include "DamageSystem.h"

NS_BEGIN(Client)

Base_Gustave_Attack::Base_Gustave_Attack()
{
}

void Base_Gustave_Attack::Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target)
{
    if (target.actor == nullptr || target.statCom == nullptr)
        return;

    const float Damage = 20;
    DamageSystem::ApplyDamage(Damage, target);

    if (attacker.heroId == HeroId::Gustave &&
        attacker.mechanismCom != nullptr &&
        skill.skillKey == "Gustave_Default_Attack")
    {
        attacker.mechanismCom->Add_GustaveCharge(1);
    }
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
