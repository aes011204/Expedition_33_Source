#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class Mon_Simon_Weapon;

class ANS_WeaponEcho : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_WeaponEcho)

public:
    ANS_WeaponEcho() = default;
    ~ANS_WeaponEcho() override = default;

public:
    string Get_TypeName() const override { return "ANS_WeaponEcho"; }

    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    Mon_Simon_Weapon* Resolve_TargetSimonWeapon(const AnimNotifyContext& context) const;
    void    Record_Sample(const AnimNotifyContext& context, bool force);
    float   Get_ElapsedSec(const AnimNotifyContext& context) const;

private:
    wstring _partTag = L"Part_Weapon";
    float   _sampleInterval = 0.03f;
    float   _delaySec = 0.2f;
    float   _startAlpha = 0.42f;
    float   _endAlpha = 0.f;
    Vec4    _tintColor = Vec4(0.70f, 1.0f, 0.92f, 1.f);

    float   _lastSampleElapsedSec = -1.f;
    float   _stateStartTimeSec = 0.f;
};

NS_END
