#include "pch.h"
#include "Base_Mon_Attack.h"

NS_BEGIN(Client)

Base_Mon_Attack::Base_Mon_Attack()
{
}

void Base_Mon_Attack::Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target)
{
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
