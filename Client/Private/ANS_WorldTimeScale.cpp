#include "ANS_WorldTimeScale.h"
#include "ClientInstance.h"
#include  "GameInstance.h"

IMPLEMENT_REFLECTION(ANS_WorldTimeScale)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("월드 시간", _worldTimeScale);
    PROPERTY_BOOL("프리뷰 이펙트 시간 적용", _scalePreviewEffects);

    return true;
}

void ANS_WorldTimeScale::On_Begin(const AnimNotifyContext& context)
{
    _restoreWorldTimeScale= GAME->Get_WorldTimeScale();
    GAME->Set_WorldTimeScale(_worldTimeScale);
}

void ANS_WorldTimeScale::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_WorldTimeScale::On_End(const AnimNotifyContext& context)
{

   GAME->Set_WorldTimeScale(_restoreWorldTimeScale);
}
