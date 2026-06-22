#include "ANS_WeaponEcho.h"
#include "ContainerObject.h"
#include "Mon_Simon_Weapon.h"

IMPLEMENT_REFLECTION(ANS_WeaponEcho)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("부위 태그", _partTag);
    PROPERTY_FLOAT("샘플 간격", _sampleInterval, 0.f, 0.2f, 0.001f);
    PROPERTY_FLOAT("지연 시간", _delaySec, 0.f, 1.0f, 0.01f);
    PROPERTY_FLOAT("시작 알파", _startAlpha, 0.f, 1.f, 0.01f);
    PROPERTY_FLOAT("종료 알파", _endAlpha, 0.f, 1.f, 0.01f);
    PROPERTY_VEC4("잔상 색상", _tintColor, 0.01f);

    return true;
}

void ANS_WeaponEcho::On_Begin(const AnimNotifyContext& context)
{
    _lastSampleElapsedSec = -1.f;
    _stateStartTimeSec = context.currentTimeSec - Get_ElapsedSec(context);

    if (Mon_Simon_Weapon* simonWeapon = Resolve_TargetSimonWeapon(context))
        simonWeapon->Begin_WeaponEchoPlayback(_delaySec, _startAlpha, _endAlpha, _tintColor);

    Record_Sample(context, true);
}

void ANS_WeaponEcho::On_Tick(const AnimNotifyContext& context)
{
    Record_Sample(context, false);
}

void ANS_WeaponEcho::On_End(const AnimNotifyContext& context)
{
    Record_Sample(context, true);

    if (Mon_Simon_Weapon* simonWeapon = Resolve_TargetSimonWeapon(context))
        simonWeapon->End_WeaponEchoPlayback();

    _lastSampleElapsedSec = -1.f;
}

Mon_Simon_Weapon* ANS_WeaponEcho::Resolve_TargetSimonWeapon(const AnimNotifyContext& context) const
{
    if (context.owner == nullptr)
        return nullptr;

    Shared<GameObject> current = context.owner->GetSharedPtr<GameObject>();
    while (current)
    {
        ContainerObject* ownerContainer = dynamic_cast<ContainerObject*>(current.get());
        if (ownerContainer != nullptr)
        {
            Mon_Simon_Weapon* weapon = dynamic_cast<Mon_Simon_Weapon*>(ownerContainer->Get_PartObject(_partTag));
            if (weapon != nullptr)
                return weapon;
        }

        current = current->Get_Owner();
    }

    return nullptr;
}

void ANS_WeaponEcho::Record_Sample(const AnimNotifyContext& context, bool force)
{
    Mon_Simon_Weapon* simonWeapon = Resolve_TargetSimonWeapon(context);
    if (simonWeapon == nullptr)
        return;

    const float elapsedSec = Get_ElapsedSec(context);
    const float interval = max(0.f, _sampleInterval);
    const bool timeMovedBackward = _lastSampleElapsedSec >= 0.f && elapsedSec + 1e-4f < _lastSampleElapsedSec;
    const bool shouldSample =
        force ||
        timeMovedBackward ||
        _lastSampleElapsedSec < 0.f ||
        interval <= 0.f ||
        elapsedSec - _lastSampleElapsedSec + 1e-4f >= interval;

    if (!shouldSample)
        return;

    simonWeapon->Record_WeaponEchoSample(elapsedSec);

    _lastSampleElapsedSec = elapsedSec;
}

float ANS_WeaponEcho::Get_ElapsedSec(const AnimNotifyContext& context) const
{
    if (context.notifyStateDurationSec > 0.f)
        return max(0.f, context.notifyStateDurationSec * context.notifyStateProgress);

    return max(0.f, context.currentTimeSec - _stateStartTimeSec);
}
