#include "pch.h"
#include "Base_Attack.h"

#include "Character.h"

Base_Attack::Base_Attack()
{
}

void Base_Attack::Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target)
{

    //DataBase->GetRowData("dataname")



    //attacker.actor->Get_Name() 



}

Shared<Base_Attack> Base_Attack::Create()
{
    return make_shared<Base_Attack>();
}

void Base_Attack::Free()
{
    SkillScript::Free();
}
