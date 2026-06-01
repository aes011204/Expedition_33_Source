#include "ANS_QTE.h"

#include "BattleController.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(ANS_QTE)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("Intro Duration", _introDuration);
    PROPERTY_FLOAT("Progress Duration", _progressDuration);
    PROPERTY_FLOAT("Perfect Start Ratio", _perfectStartRatio);
    PROPERTY_FLOAT("Perfect End Ratio", _perfectEndRatio);

    return true;
}

void ANS_QTE::On_Begin(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    BattleQTERequest request{};
    request.introDuration = _introDuration;
    request.progressDuration = _progressDuration;
    request.perfectStartRatio = _perfectStartRatio;
    request.perfectEndRatio = _perfectEndRatio;

    battle->Begin_QTE(request);
}

void ANS_QTE::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_QTE::On_End(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    // Notify State 구간이 끝났는데 아직 입력 결과가 없으면 실패로 확정
    battle->Fail_QTEIfActive();
}
