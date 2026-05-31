#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class Weapon;

class ANS_WeaponRotate : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_WeaponRotate)

public:
    enum class WeaponRotateAxis : uint8
    {
        Right,
        Up,
        Forward,
        Custom
    };

public:
    ANS_WeaponRotate() = default;
    ~ANS_WeaponRotate() override = default;

public:
    string Get_TypeName() const override { return "ANS_WeaponRotate"; }

    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;
    void From_Json(const json& data) override;

private:
    // Notify Owner가 Part일 수도 있어서
    Weapon* Resolve_TargetWeapon(const AnimNotifyContext& context) const;
    void    Apply_RotationFromContext(Weapon& weapon, const AnimNotifyContext& context) const;
    Vec3    Resolve_AxisLocal() const;

private:
    wstring _partTag = L"Part_WeaponL";      // 구스타브 총 기본값
    WeaponRotateAxis _axis = WeaponRotateAxis::Forward; // 사용자는 보통 축 이름만 고르면 된다.
    Vec3    _customAxisLocal = Vec3(0.f, 0.f, 1.f);    // Custom일 때만 직접 축 벡터를 쓴다.
    float   _degreesPerSecond = 720.f;
    float   _startAngleDegree = 0.f;

};

NS_END
