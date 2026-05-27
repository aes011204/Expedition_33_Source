#include "Base_Mon_Defense.h"

#include "CombatStatCom.h"

Base_Mon_Defense::Base_Mon_Defense()
{
}

void Base_Mon_Defense::Execute(const Skill_Data& skill, const Action& action)
{
    uint32 shieldNum = {5};

    action ._curChar.statCom->Add_Shield(shieldNum);

}

Shared<Base_Mon_Defense> Base_Mon_Defense::Create()
{
    return make_shared<Base_Mon_Defense>();
}

void Base_Mon_Defense::Free()
{
    SkillScript::Free();
}
