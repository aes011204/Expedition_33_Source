#include "ANS_Parry.h"

#include "ANS_WeaponEcho.h"
#include "BattleController.h"
#include "GameInstance.h"
#include "ClientInstance.h"


IMPLEMENT_REFLECTION(ANS_Parry)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("테스트 값", _testValue);

    return true;
}

void ANS_Parry::On_Begin(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    const BattleAction& turnAction = battle->Get_TurnAction();

    battle->Get_ReactionController().OpenWindow(
        BattleReactionType::Parry,
        turnAction,
        context.notifyStateDurationSec);

    ANS_WeaponEcho::Queue_EchoReactionWindowsFromNotify(
        context,
        BattleReactionType::Parry,
        turnAction,
        context.notifyStateDurationSec);
}

void ANS_Parry::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
    
}

void ANS_Parry::On_End(const AnimNotifyContext& context)
{
    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    battle->Get_ReactionController().CloseWindow(BattleReactionType::Parry);
}
