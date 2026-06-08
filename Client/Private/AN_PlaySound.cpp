#include "AN_PlaySound.h"

#include "GameInstance.h"
#include "PartObject.h"

IMPLEMENT_REFLECTION(AN_PlaySound)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("Sound Cue", _soundCue);
    PROPERTY_WSTRING("Sound File", _soundFile);
    PROPERTY_ENUM("Channel", _channel, SoundChannel);
    PROPERTY_FLOAT("Volume", _volume, 0.f, 1.f, 0.01f);
    PROPERTY_FLOAT("Pitch", _pitch, 0.01f, 3.f, 0.01f);

    return true;
}

void AN_PlaySound::Execute(const AnimNotifyContext& context)
{
    Vec3 ownerPosition = Vec3::Zero;
    const Vec3* soundPosition = nullptr;

    if (context.owner && context.owner->Get_Transform())
    {
        if (const auto* partOwner = dynamic_cast<PartObject*>(context.owner))
        {
            const Matrix& partWorld = partOwner->Get_CombinedWorldMatrix();
            ownerPosition = Vec3(partWorld._41, partWorld._42, partWorld._43);
        }
        else
        {
            ownerPosition = context.owner->Get_Transform()->Get_WorldPosition();
        }

        soundPosition = &ownerPosition; // spatial cue일 때 감쇠 기준 위치
    }

    if (!_soundCue.empty())
    {
        GAME->Play_SoundCue(_soundCue, soundPosition);
        return;
    }

    if (_soundFile.empty())
        return;

    if (_pitch == 1.f)
    {
        GAME->Play_Sound(_soundFile, _channel, _volume);
        return;
    }

    GAME->Play_Sound_Pitched(_soundFile, _channel, _volume, _pitch);
}
