#include "AN_CameraShake.h"
#include "Battle_Event.h"
#include "Event_Bus.h"
#include "GameInstance.h"

IMPLEMENT_REFLECTION(AN_CameraShake)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("amplitude", _amplitude);
    PROPERTY_FLOAT("duration", _duration);
    PROPERTY_FLOAT("strength scale", _strengthScale);
    PROPERTY_FLOAT("vertical ratio", _verticalRatio);
    PROPERTY_FLOAT("frequencyX",_frequencyX);
    PROPERTY_FLOAT("frequencyY", _frequencyY);
    PROPERTY_FLOAT("decayRateX", _decayRateX);
    PROPERTY_FLOAT("decayRateY", _decayRateY);

    return true;
}

void AN_CameraShake::Execute(const AnimNotifyContext& context)
{
    EvtBattleCameraShake event {};
    event.amplitude = _amplitude;
    event.duration = _duration;
    event.strengthScale = _strengthScale;
    event.verticalRatio = _verticalRatio;
    event.frequencyX = _frequencyX;
    event.frequencyY = _frequencyY;
    event.decayRateX = _decayRateX;
    event.decayRateY = _decayRateY;
    GAME->Get_EventBus().Publish(event);
}
