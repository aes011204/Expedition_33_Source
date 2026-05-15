
#include "ANS_QTE.h"

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
}

void ANS_QTE::On_End(const AnimNotifyContext& context)
{
}
