#include "AN_SimonCriticalGlow.h"
#include "ClientInstance.h"
#include "SimonCriticalGlowController.h"

IMPLEMENT_REFLECTION(AN_SimonCriticalGlow)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BOOL("critical", _critical);
    PROPERTY_FLOAT("duration", _duration);

    return true;
}

void AN_SimonCriticalGlow::Execute(const AnimNotifyContext& context)
{
    if (SimonCriticalGlowController* glowController = CLIENT->Get_SimonCriticalGlowController())
    {
        glowController->Set_Critical(_critical, _duration);
    }
}
