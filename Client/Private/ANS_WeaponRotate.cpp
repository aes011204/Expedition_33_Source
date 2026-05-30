#include "ANS_WeaponRotate.h"
#include "ContainerObject.h"
#include "Weapon.h"

REGISTER_GLOBAL_ENUM(Client::ANS_WeaponRotate::WeaponRotateAxis, "WeaponRotateAxis")

IMPLEMENT_REFLECTION(ANS_WeaponRotate)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("대상 파츠", _partTag);
    PROPERTY_ENUM("회전 축", _axis, WeaponRotateAxis);
    PROPERTY_VEC3("커스텀 로컬 축", _customAxisLocal, 0.1f);
    PROPERTY_FLOAT("초당 회전 각도", _degreesPerSecond, -3600.f, 3600.f, 1.f);
    PROPERTY_FLOAT("시작 각도", _startAngleDegree, -3600.f, 3600.f, 1.f);

    return true;
}

void ANS_WeaponRotate::On_Begin(const AnimNotifyContext& context)
{
    Weapon* weapon = Resolve_TargetWeapon(context);
    if (weapon == nullptr)
        return;

    if (!weapon->Begin_NotifyRotation())
        return;

    Apply_RotationFromContext(*weapon, context);
}

void ANS_WeaponRotate::On_Tick(const AnimNotifyContext& context)
{
    Weapon* weapon = Resolve_TargetWeapon(context);
    if (weapon == nullptr)
        return;

    Apply_RotationFromContext(*weapon, context);
}

void ANS_WeaponRotate::On_End(const AnimNotifyContext& context)
{
    Weapon* weapon = Resolve_TargetWeapon(context);
    if (weapon == nullptr)
        return;

    weapon->End_NotifyRotation();
}

void ANS_WeaponRotate::From_Json(const json& data)
{
    __super::From_Json(data);

    if (!data.contains("_axis") && data.contains("_axisLocal"))
    {
        const json& axisNode = data["_axisLocal"];
        if (axisNode.is_array() && axisNode.size() >= 3)
        {
            _customAxisLocal.x = axisNode[0].get<float>();
            _customAxisLocal.y = axisNode[1].get<float>();
            _customAxisLocal.z = axisNode[2].get<float>();
            _axis = WeaponRotateAxis::Custom; // 예전 _axisLocal 데이터는 Custom 축으로 받아서 기존 notify를 최대한 유지한다.
        }
    }
}

Weapon* ANS_WeaponRotate::Resolve_TargetWeapon(const AnimNotifyContext& context) const
{
    if (context.owner == nullptr)
        return nullptr;

    Shared<GameObject> current = context.owner->GetSharedPtr<GameObject>();
    while (current)
    {
        ContainerObject* ownerContainer = dynamic_cast<ContainerObject*>(current.get());
        if (ownerContainer != nullptr)
        {
            Weapon* weapon = dynamic_cast<Weapon*>(ownerContainer->Get_PartObject(_partTag));
            if (weapon != nullptr)
                return weapon; // preview에서는 model owner가 한 단계 더 안쪽일 수 있어서 owner chain 전체를 끝까지 확인한다.
        }

        current = current->Get_Owner();
    }

    return nullptr;
}

void ANS_WeaponRotate::Apply_RotationFromContext(Weapon& weapon, const AnimNotifyContext& context) const
{
    // delta 누적이 아니라 절대시간 기준으로 계산
    const float elapsedSec = context.notifyStateDurationSec * context.notifyStateProgress;
    const float angleDegree = _startAngleDegree + (_degreesPerSecond * elapsedSec);

    weapon.Apply_NotifyRotation(Resolve_AxisLocal(), angleDegree);
}

Vec3 ANS_WeaponRotate::Resolve_AxisLocal() const
{
    switch (_axis)
    {
    case WeaponRotateAxis::Right:
        return Vec3(1.f, 0.f, 0.f);

    case WeaponRotateAxis::Up:
        return Vec3(0.f, 1.f, 0.f);

    case WeaponRotateAxis::Forward:
        return Vec3(0.f, 0.f, 1.f);

    case WeaponRotateAxis::Custom:
        return _customAxisLocal;
    }

    return Vec3(0.f, 0.f, 1.f);
}
