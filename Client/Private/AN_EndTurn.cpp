#include "AN_EndTurn.h"

#include "BattleAnimutils.h"
#include "ModelCom.h"
#include "BattleController.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"

IMPLEMENT_REFLECTION(AN_EndTurn)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    return true;
}

void AN_EndTurn::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    const auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr || !battle->Is_BattleStarted())
        return;

    auto curChar = battle->Get_TurnAction().Get_CurChar();

    if(curChar.statCom == nullptr)
        return;

    if (curChar.statCom->Get_CurAliveState() == EAliveStatus::Dead)
        return;

    BattleAnimUtils::Play_BattleIdle(
        battle->Get_TurnAction().Get_CurChar(), 0.2f);

    battle->EndTurn();
}
