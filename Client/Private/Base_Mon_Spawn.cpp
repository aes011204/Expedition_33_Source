#include "Base_Mon_Spawn.h"

#include "Monster_BattleBase.h"
#include "ClientInstance.h"

Base_Mon_Spawn::Base_Mon_Spawn()
{
}

void Base_Mon_Spawn::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    UNREFERENCED_PARAMETER(skill);
    UNREFERENCED_PARAMETER(action);
    UNREFERENCED_PARAMETER(attackType);

    auto enemy = CLIENT->Get_BattleController()->Get_AliveEnemies();
    if(enemy.size() == 1)
    {
        auto right = enemy[0].actor->Get_Transform()->Get_WorldRight();
        auto pos = enemy[0].actor->Get_Transform()->Get_Position();
        CLIENT->Get_BattleController()->Spawn_BattleChar(false, "MB_Lancelier", pos + right * 3.f );
        CLIENT->Get_BattleController()->Spawn_BattleChar(false, "MB_Lancelier", pos - right * 3.f);

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
