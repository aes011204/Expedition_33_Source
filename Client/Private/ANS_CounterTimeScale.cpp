#include "ANS_CounterTimeScale.h"

#include "BattleController.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(ANS_CounterTimeScale)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("월드 시간", _worldTimeScale);
    PROPERTY_FLOAT("애니메이션 재생 시간", _counterFrameSpeed);

    return true;
}

void ANS_CounterTimeScale::On_Begin(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle)
        battle->BeginCounterTimeSection(context.model, _worldTimeScale, _counterFrameSpeed);
}

void ANS_CounterTimeScale::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_CounterTimeScale::On_End(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle)
        battle->EndCounterTimeSection(context.model);
}
