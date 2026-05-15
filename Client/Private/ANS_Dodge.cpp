#include "ANS_Dodge.h"

#include "GameInstance.h"


IMPLEMENT_REFLECTION(ANS_Dodge)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("테스트 값", _testValue);

    return true;
}

void ANS_Dodge::On_Begin(const AnimNotifyContext& context)
{
}

void ANS_Dodge::On_Tick(const AnimNotifyContext& context)
{
    LOG_INFO({ "DODGE TIME" });

    if (GAME->KeyDown(KEY_TYPE::Q))
    {
        LOG_ERROR({ " SUCCESS  DODGE" });

    }
}

void ANS_Dodge::On_End(const AnimNotifyContext& context)
{
}
