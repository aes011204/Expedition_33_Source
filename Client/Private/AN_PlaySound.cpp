#include "AN_PlaySound.h"

#include "GameInstance.h"
#include "PartObject.h"

IMPLEMENT_REFLECTION(AN_PlaySound)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("Sound Cue", _soundCue);
    PROPERTY_FLOAT("Volume", _volume, 0.f, 10.f, 0.01f);
    PROPERTY_FLOAT("Speed", _speed, 0.1f, 3.f, 0.01f);
    PROPERTY_FLOAT("Start Ratio", _startRatio, 0.f, 0.99f, 0.01f);

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

    GAME->Play_SoundCue(_soundCue, soundPosition, _volume, _speed, _startRatio);
}

void AN_PlaySound::From_Json(const json& data)
{
    __super::From_Json(data);

    if (_soundCue.empty())
        return;

    const bool hasVolume = data.contains("_volume");
    const bool hasSpeed = data.contains("_speed");
    const bool hasLegacyPitch = data.contains("_pitch");
    if (!hasSpeed && hasLegacyPitch)
        _speed = data["_pitch"].get<float>(); // 기존 notify의 _pitch 값은 이제 피치가 아니라 재생 속도로 승계한다.

    if (hasVolume && (hasSpeed || hasLegacyPitch))
        return;

    float cueVolume = 1.f;
    float cueSpeed = 1.f;
    if (!GAME->Try_GetSoundCueDefaultVolumeSpeed(_soundCue, cueVolume, cueSpeed))
        return;

    if (!hasVolume)
        _volume = cueVolume; // 예전 notify는 cue 테이블 값을 최초 커스텀 기본값으로 받아온다.

    if (!hasSpeed && !hasLegacyPitch)
        _speed = cueSpeed;
}
