#include "AN_PlaySound.h"

#include "GameInstance.h"
#include "PartObject.h"

IMPLEMENT_REFLECTION(AN_PlaySound)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("Sound Cue", _soundCue);
    PROPERTY_FLOAT("Volume", _volume, 0.f, 10.f, 0.01f);
    PROPERTY_FLOAT("Pitch", _pitch, 0.1f, 3.f, 0.01f);

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

    if (_soundCue.empty())
        return;

    GAME->Play_SoundCue(_soundCue, soundPosition, _volume, _pitch);
}

void AN_PlaySound::From_Json(const json& data)
{
    __super::From_Json(data);

    if (_soundCue.empty())
        return;

    const bool hasVolume = data.contains("_volume");
    const bool hasPitch = data.contains("_pitch");
    if (hasVolume && hasPitch)
        return;

    float cueVolume = 1.f;
    float cuePitch = 1.f;
    if (!GAME->Try_GetSoundCueDefaultVolumePitch(_soundCue, cueVolume, cuePitch))
        return;

    if (!hasVolume)
        _volume = cueVolume; // 예전 notify는 cue 테이블 값을 최초 커스텀 기본값으로 받아온다.

    if (!hasPitch)
        _pitch = cuePitch;
}
