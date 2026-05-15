#include "ANS_Parry.h"
#include "GameInstance.h"


IMPLEMENT_REFLECTION(ANS_Parry)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("테스트 값", _testValue);

    return true;
}

void ANS_Parry::On_Begin(const AnimNotifyContext& context)
{
}

void ANS_Parry::On_Tick(const AnimNotifyContext& context)
{
    LOG_INFO({ "PARRY TIME" });

    if(GAME->KeyDown(KEY_TYPE::E))
    {
        LOG_ERROR({ " SUCCESS  PARRY" });

    }
}

void ANS_Parry::On_End(const AnimNotifyContext& context)
{
}
