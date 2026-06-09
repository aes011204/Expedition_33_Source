#include "AN_PlaySound.h"

#include "GameInstance.h"
#include "PartObject.h"

IMPLEMENT_REFLECTION(AN_PlaySound)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("Sound Cue", _soundCue);

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

    GAME->Play_SoundCue(_soundCue, soundPosition);
}
