#include "ANS_Dodge.h"

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
    auto battle = CLIENT->Get_BattleController();
    if (battle)
        battle->OpenReactionWindow(BattleController::ReactionType::Dodge);
}

void ANS_Dodge::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
    LOG_DEBUG("dodgeeeeeeeeeeeeeeeee");

}

void ANS_Dodge::On_End(const AnimNotifyContext& context)
{
    auto battle = CLIENT->Get_BattleController();
    if (battle)
        battle->CloseReactionWindow(BattleController::ReactionType::Dodge);
}
