#include "ANS_Parry.h"

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
    auto battle = CLIENT->Get_BattleController();
    if (battle)
        battle->OpenReactionWindow(BattleController::ReactionType::Parry);
}

void ANS_Parry::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
    LOG_DEBUG("parryyyyyyyyyyyy");
}

void ANS_Parry::On_End(const AnimNotifyContext& context)
{
    auto battle = CLIENT->Get_BattleController();
    if (battle)
        battle->CloseReactionWindow(BattleController::ReactionType::Parry);
}
