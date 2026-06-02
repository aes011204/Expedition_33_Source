#include "pch.h"
#include "Base_Gustave_Attack.h"

#include "BattleMechanismCom.h"
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

    auto attacker = action.Get_CurChar();

    DamageSystem::ApplyDamage(Damage, attacker, action.GetCurStepTarget(), attackType);

    if (attacker.heroId == HeroId::Gustave &&
        attacker.mechanismCom != nullptr &&
        skill.skillKey == "Gustave_Default_Attack")
    {
        attacker.mechanismCom->Add_GustaveCharge(1);
    }

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
