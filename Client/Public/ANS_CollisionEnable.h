#pragma once

#include "AnimNotifyState.h"
#include "SL_Types.h"

NS_BEGIN(Client)

class ANS_CollisionEnable final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_CollisionEnable)

public:
    ANS_CollisionEnable() = default;
    ~ANS_CollisionEnable() override = default;

public:
    string Get_TypeName() const override { return "ANS_CollisionEnable"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    Shared<GameObject> Resolve_CollisionOwner(GameObject* notifyOwner) const;
    Shared<ColliderCom> Resolve_Collider(const Shared<GameObject>& owner) const;
    void Sync_Collider(const Shared<GameObject>& owner, const Shared<ColliderCom>& collider) const;
    void Set_ColliderActive(const AnimNotifyContext& context, bool active) const;

private:
    string _colliderTag = "Com_SL_PlayerAttack";

    SL_HitAnimOverride _hitAnimOverride = SL_HitAnimOverride::Hit_Light;
};

NS_END
