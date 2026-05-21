#include "AN_Attack.h"

#include "BattleController.h"
#include "ClientInstance.h"
#include "SkillScript.h"

void AN_Attack::Execute(const AnimNotifyContext& context)
{
    auto battle = CLIENT->Get_BattleController();
    if (battle->Get_CurrentScript() == nullptr)
        return;


    // Free Aim은 스킵
    if (battle->Is_FreeAimShotPlaying())
        return;

    // 닷지, 패링 성공시 리턴 
    if (battle->Get_Reaction().isConsumed == true)
    {
        battle->ClearReactionState();
        return;

    }

    battle->Get_CurrentScript()->Execute(battle->Get_TurnAction()._selected_SkillKey, battle->Get_TurnAction()._curChar, battle->Get_TurnAction().target);
   battle->HitAction(battle->Get_TurnAction().target);

}
