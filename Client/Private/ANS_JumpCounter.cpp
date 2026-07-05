#include "ANS_JumpCounter.h"

#include "BattleController.h"
#include "GameInstance.h"
#include "ClientInstance.h"


IMPLEMENT_REFLECTION(ANS_JumpCounter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("JumpCounter TimeScale", _jumpCounterWorldTimeScale);

    return true;
}

void ANS_JumpCounter::On_Begin(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    _startedCounter = false;
    battle->Get_ReactionController().NotifyJumpCounterWindowBegin(_jumpCounterWorldTimeScale);
}

void ANS_JumpCounter::On_Tick(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    if (_startedCounter)
        return;

    _startedCounter = battle->Get_ReactionController().TryStartQueuedJumpCounter(context.model);
}

void ANS_JumpCounter::On_End(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    if (!_startedCounter)
    {
        // 윈도우 끝 프레임에 입력이 성공하면 다음 Tick이 오기 전에 End가 먼저 올 수 있다.
        _startedCounter = battle->Get_ReactionController().TryStartQueuedJumpCounter(context.model);
    }

    battle->Get_ReactionController().NotifyJumpCounterWindowEnd();

}
