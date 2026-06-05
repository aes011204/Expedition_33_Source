#include "AN_PlaySound.h"

#include "GameInstance.h"

IMPLEMENT_REFLECTION(AN_PlaySound)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("Sound File", _soundFile);
    PROPERTY_ENUM("Channel", _channel, SoundChannel);
    PROPERTY_FLOAT("Volume", _volume, 0.f, 1.f, 0.01f);
    PROPERTY_FLOAT("Pitch", _pitch, 0.01f, 3.f, 0.01f);

    return true;
}

void AN_PlaySound::Execute(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);

    if (_soundFile.empty())
        return;

    if (_pitch == 1.f)
    {
        GAME->Play_Sound(_soundFile, _channel, _volume);
        return;
    }

    GAME->Play_Sound_Pitched(_soundFile, _channel, _volume, _pitch);
}
