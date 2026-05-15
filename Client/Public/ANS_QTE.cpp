
#include "ANS_QTE.h"
#include "GameInstance.h"

IMPLEMENT_REFLECTION(ANS_QTE)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("테스트 값", _testValue);

    return true;
}

void ANS_QTE::On_Begin(const AnimNotifyContext& context)
{
}

void ANS_QTE::On_Tick(const AnimNotifyContext& context)
{
    LOG_INFO({ "QTE TIME" });

    if (GAME->KeyDown(KEY_TYPE::SPACE))
    {
        LOG_ERROR({ " SUCCESS  QTE" });

    }
}

void ANS_QTE::On_End(const AnimNotifyContext& context)
{
}
