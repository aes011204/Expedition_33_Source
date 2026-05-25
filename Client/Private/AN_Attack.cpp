#include "AN_Attack.h"

#include "BattleController.h"
#include "ClientInstance.h"
#include "SkillScript.h"

void AN_Attack::Execute(const AnimNotifyContext& context)
{
    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    if (battle->Get_CurrentScript() == nullptr)
        return;

    // Free Aim은 스킵
    if (battle->Is_FreeAimShotPlaying())
        return;

    Action& turnAction = battle->Get_TurnAction();

    // 회피/패링 성공 입력이 있으면 이번 공격 notify의 데미지를 소비하고 스킵한다.
    if (battle->Try_ConsumeReactionGuard(turnAction.target))
        return;


    battle->Get_CurrentScript()->Execute(
        turnAction._selected_SkillKey,
        turnAction._curChar,
        turnAction.target);

   battle->HitAction(turnAction.target);
}
