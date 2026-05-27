#include "Base_Mon_Defense.h"

#include "CombatStatCom.h"

Base_Mon_Defense::Base_Mon_Defense()
{
}

void Base_Mon_Defense::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    UNREFERENCED_PARAMETER(skill);
    UNREFERENCED_PARAMETER(attackType);

    uint32 shieldNum = {5};

    if (action.Get_CurChar().statCom)
        action.Get_CurChar().statCom->Add_Shield(shieldNum);

}

Shared<Base_Mon_Defense> Base_Mon_Defense::Create()
{
    return make_shared<Base_Mon_Defense>();
}

void Base_Mon_Defense::Free()
{
    SkillScript::Free();
}
