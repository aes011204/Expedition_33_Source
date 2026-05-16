#include "AN_Test2.h"

#include "GameObject.h"
#include "InputCom.h"

IMPLEMENT_REFLECTION(AN_Test2)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_INT("테스트 값", _testInt);
    PROPERTY_VEC3("테스트 벡터", _testVec3);
    
    return true;
}

void AN_Test2::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (!context.owner || !context.model)
        return;

    
}
