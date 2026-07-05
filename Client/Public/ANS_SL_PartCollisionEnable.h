#pragma once

#include "AnimNotifyState.h"
#include "SL_Types.h"

NS_BEGIN(Engine)
class ColliderCom;
class ContainerObject;
class PartObject;
NS_END

NS_BEGIN(Client)

class ANS_SL_PartCollisionEnable final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_SL_PartCollisionEnable)

public:
    ANS_SL_PartCollisionEnable() = default;
    ~ANS_SL_PartCollisionEnable() override = default;

public:
    string Get_TypeName() const override { return "ANS_SL_PartCollisionEnable"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    Shared<ContainerObject> Resolve_OwnerContainer(GameObject* notifyOwner) const;
    Shared<PartObject> Resolve_Part(const Shared<ContainerObject>& ownerContainer) const;
    Shared<ColliderCom> Resolve_Collider(const Shared<PartObject>& part) const;
    void Set_ColliderActive(const AnimNotifyContext& context, bool active) const;

private:
    string _partTag = "Part_Weapon";
    string _colliderTag = "Com_SL_MonsterSword_R";
    SL_HitAnimOverride _hitAnimOverride = SL_HitAnimOverride::Hit_Light;
    wstring _hitEffectName{};
    Vec3 _hitEffectLocalOffset = Vec3::Zero;
    Vec3 _hitEffectScale = Vec3::One;

    float _damageMultiplier = 1.f;
};

NS_END
