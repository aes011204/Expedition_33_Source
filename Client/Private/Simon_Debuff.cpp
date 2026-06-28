#include "pch.h"
#include "Simon_Debuff.h"

#include "ClientInstance.h"
#include "CombatStatCom.h"

NS_BEGIN(Client)

Simon_Debuff::Simon_Debuff()
{
}

void Simon_Debuff::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    UNREFERENCED_PARAMETER(skill);
    UNREFERENCED_PARAMETER(attackType);

    for (const BattleChar& target : action.GetCurStepTarget())
    {
        if (target.statCom == nullptr)
            continue;

        if (target.statCom->Get_AliveStatus() != EAliveStatus::Dead)
            continue;

        target.statCom->Set_ReviveBlocked(true);
    }

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
