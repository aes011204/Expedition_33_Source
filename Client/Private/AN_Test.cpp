#include "AN_Test.h"
#include "GameObject.h"

IMPLEMENT_REFLECTION(AN_Test)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_INT("테스트 값", _testInt);
    PROPERTY_VEC3("테스트 벡터", _testVec3);

    return true;
}

void AN_Test::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (!context.owner || !context.model)
        return;

    LOG_WARN("AN_Test 들어왔다.");
}
