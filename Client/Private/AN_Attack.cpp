#include "AN_Attack.h"

#include "BattleController.h"
#include "ClientInstance.h"
#include "SkillScript.h"

void AN_Attack::Execute(const AnimNotifyContext& context)
{
   auto battle =  CLIENT->Get_BattleController();
   if (battle->Get_CurrentScript() == nullptr)
       return;

   battle->Get_CurrentScript()->Execute(battle->Get_TurnAction()._selected_SkillKey, battle->Get_TurnAction()._curChar, battle->Get_TurnAction().target);
}
