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
    LOG_INFO({ "PARRY TIME" });

   
}

void ANS_Parry::On_End(const AnimNotifyContext& context)
{
    auto battle = CLIENT->Get_BattleController();
    if (battle)
        battle->OpenReactionWindow(BattleController::ReactionType::Parry);
}
