#include "ANS_Test.h"

IMPLEMENT_REFLECTION(ANS_Test)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("테스트 값", _testValue);

    return true;
}

void ANS_Test::On_Begin(const AnimNotifyContext& context)
{
    if (!context.owner || !context.model)
        return;

    LOG_WARN("ANS_Test Begin");
}

void ANS_Test::On_Tick(const AnimNotifyContext& context)
{
}

void ANS_Test::On_End(const AnimNotifyContext& context)
{
    if (!context.owner || !context.model)
        return;

    LOG_WARN("ANS_Test End");
}
