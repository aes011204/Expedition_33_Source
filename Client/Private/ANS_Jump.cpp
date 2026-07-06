#include "ANS_Jump.h"

#include "GameInstance.h"
#include "BattleController.h"
#include "ClientInstance.h"


IMPLEMENT_REFLECTION(ANS_Jump)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("테스트 값", _testValue);

    return true;
}

void ANS_Jump::On_Begin(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    battle->Get_ReactionController().OpenWindow(BattleReactionType::Jump);
}

void ANS_Jump::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_Jump::On_End(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    battle->Get_ReactionController().CloseWindow(BattleReactionType::Jump);
}
