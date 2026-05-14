
#include "DamageSystem.h"

#include "Battle_Enum.h"
#include "CombatStatCom.h"


void DamageSystem::Heal(float amount, const BattleChar& target)
{
    auto& runtimeStat = target.statCom->Get_RuntimeStat();
        runtimeStat.CurHp += amount;

    if (runtimeStat.CurHp > target.statCom->Get_StatInfo().maxHp)
    {
        runtimeStat.CurHp = target.statCom->Get_StatInfo().maxHp;
    }
}

void DamageSystem::ApplyDamage(float damage, const BattleChar& target)
{
   
    auto& runtimeStat = target.statCom->Get_RuntimeStat();
    if (runtimeStat.CurHp < 0)
    {
        runtimeStat.CurHp -= damage;
    }
    else
    {
        runtimeStat.CurHp = 0;
    }
}
