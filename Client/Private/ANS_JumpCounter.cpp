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

    _startedCounter = false;
    battle->Get_ReactionController().NotifyJumpCounterWindowBegin();
}

void ANS_JumpCounter::On_Tick(const AnimNotifyContext& context)
{
    LOG_INFO("JUMPCOUNTER");

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    if (_startedCounter)
        return;

    _startedCounter = battle->Get_ReactionController().TryStartQueuedJumpCounter(context.model);
}

void ANS_JumpCounter::On_End(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;
    battle->Get_ReactionController().NotifyJumpCounterWindowEnd();

}
