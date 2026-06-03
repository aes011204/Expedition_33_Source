#include "Base_Mon_Spawn.h"

#include "Monster_BattleBase.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"

Base_Mon_Spawn::Base_Mon_Spawn()
{
}

void Base_Mon_Spawn::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    UNREFERENCED_PARAMETER(skill);
    UNREFERENCED_PARAMETER(action);
    UNREFERENCED_PARAMETER(attackType);

    auto allEnemy = CLIENT->Get_BattleController()->Get_AllEnemies();
    auto aliveEnemy = CLIENT->Get_BattleController()->Get_AliveEnemies();
    if(allEnemy.size()==3)
    {
        //
        for(auto& it : allEnemy)
        {
            if(it.statCom->Get_CurAliveState()==EAliveStatus::Dead)
            {
                
                CLIENT->Get_BattleController()->Revive_BattleMon(it, 1.f);
            }
        }
    }
     else if (allEnemy.size() == 1)
     {
         auto right = aliveEnemy[0].actor->Get_Transform()->Get_WorldRight();
         auto pos = aliveEnemy[0].actor->Get_Transform()->Get_Position();
         CLIENT->Get_BattleController()->Spawn_BattleChar(false, "MB_Lancelier", pos + right * 10.f);
         CLIENT->Get_BattleController()->Spawn_BattleChar(false, "MB_Lancelier", pos - right * 10.f);

    }
}

Shared<Base_Mon_Spawn> Base_Mon_Spawn::Create()
{
    return make_shared<Base_Mon_Spawn>();

}

void Base_Mon_Spawn::Free()
{
    SkillScript::Free();
}
