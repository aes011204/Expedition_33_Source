#include "ANS_Dodge.h"

#include "ANS_WeaponEcho.h"
#include "GameInstance.h"
#include "BattleController.h"
#include "ClientInstance.h"


IMPLEMENT_REFLECTION(ANS_Dodge)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("테스트 값", _testValue);

    return true;
}

void ANS_Dodge::On_Begin(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic)
        return;

    if (context.owner == nullptr || context.model == nullptr)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr || battle->Is_BattleStarted() == false)
        return;

    const BattleAction& turnAction = battle->Get_TurnAction();
    const BattleChar& actionActor = turnAction.Get_CurChar();

    if (actionActor.actor == nullptr || actionActor.ModelCom.get() != context.model)
        return;

    if (dynamic_pointer_cast<GameObject>(actionActor.actor) != context.owner->Get_Owner())
        return;

    if (turnAction.GetCurStepIndex() >= turnAction.GetSteps().size())
        return;

    battle->Get_ReactionController().OpenWindow(
        BattleReactionType::Dodge,
        turnAction,
        context.notifyStateDurationSec);

    ANS_WeaponEcho::Queue_EchoReactionWindowsFromNotify(
        context,
        BattleReactionType::Dodge,
        turnAction,
        context.notifyStateDurationSec);
}

void ANS_Dodge::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_Dodge::On_End(const AnimNotifyContext& context)
{
    if (context.isCinematic || context.owner == nullptr || context.model == nullptr)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    battle->Get_ReactionController().CloseWindow(BattleReactionType::Dodge);
}
