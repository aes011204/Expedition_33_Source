#include "AN_EnterAnim.h"

#include <ModelCom.h>

#include "BattleAnimutils.h"
#include "BattleController.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(AN_EnterAnim)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

 

    return true;
}

void AN_EnterAnim::Execute(const AnimNotifyContext& context)
{
    const auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    battle->End_FirstAnim();

    BattleAnimUtils::Play_BattleIdle(
        battle->Get_TurnAction()._curChar, 0.3f); 
}
