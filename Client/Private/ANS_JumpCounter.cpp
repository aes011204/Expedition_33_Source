#include "ANS_JumpCounter.h"

#include "BattleController.h"
#include "GameInstance.h"
#include "ClientInstance.h"


IMPLEMENT_REFLECTION(ANS_JumpCounter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("테스트 값", _testValue);

    return true;
}

void ANS_JumpCounter::On_Begin(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    battle->Get_ReactionController().OpenWindow(BattleReactionType::JumpCounter);
}

void ANS_JumpCounter::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);

    LOG_INFO("JUMPCOUNTER");
}

void ANS_JumpCounter::On_End(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);

      auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;
    battle->Get_ReactionController().CloseWindow(BattleReactionType::JumpCounter);

  //  GAME->Set_WorldTimeScale(1.f);
}
