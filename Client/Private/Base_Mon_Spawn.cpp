#include "Base_Mon_Spawn.h"

Base_Mon_Spawn::Base_Mon_Spawn()
{
}

void Base_Mon_Spawn::Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target)
{
}

Shared<Base_Mon_Spawn> Base_Mon_Spawn::Create()
{
    return make_shared<Base_Mon_Spawn>();

}

void Base_Mon_Spawn::Free()
{
    SkillScript::Free();
}
