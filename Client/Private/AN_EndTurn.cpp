#include "AN_EndTurn.h"

#include "BattleAnimutils.h"
#include "ModelCom.h"
#include "BattleController.h"
#include "ClientInstance.h"

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

    BattleAnimUtils::Play_BattleIdle(
        battle->Get_TurnAction()._curChar, 0.2f);

    battle->EndTurn();
}
