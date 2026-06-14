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
    if (battle == nullptr)
        return;

    battle->Get_ReactionController().OpenWindow(BattleReactionType::Dodge);
}

void ANS_Dodge::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_Dodge::On_End(const AnimNotifyContext& context)
{
    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    battle->Get_ReactionController().CloseWindow(BattleReactionType::Dodge);
}
