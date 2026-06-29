#include "ANS_CollisionEnable.h"

#include "ColliderCom.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "SL_Base.h"
#include "TransformCom.h"

REGISTER_GLOBAL_ENUM(SL_HitAnimOverride, "SL_HitAnimOverride")

IMPLEMENT_REFLECTION(ANS_CollisionEnable)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ANS_CollisionEnable";
    info.category = "SoulLike";

    PROPERTY_STRING("Collider Tag", _colliderTag);
    PROPERTY_ENUM("Hit Reaction", _hitAnimOverride, SL_HitAnimOverride);
    PROPERTY_WSTRING("Hit Effect", _hitEffectName);
    PROPERTY_VEC3("Hit Effect Local Offset", _hitEffectLocalOffset);
    PROPERTY_VEC3("Hit Effect Scale", _hitEffectScale);

    return true;
}

void ANS_CollisionEnable::On_Begin(const AnimNotifyContext& context)
{
    Set_ColliderActive(context, true);
}

void ANS_CollisionEnable::On_Tick(const AnimNotifyContext& context)
{
    const Shared<GameObject> owner = Resolve_CollisionOwner(context.owner);
    const Shared<ColliderCom> collider = Resolve_Collider(owner);

    if (owner == nullptr || collider == nullptr || !collider->Get_IsActive())
        return;

    Sync_Collider(owner, collider);
}

void ANS_CollisionEnable::On_End(const AnimNotifyContext& context)
{
    Set_ColliderActive(context, false);
}

Shared<GameObject> ANS_CollisionEnable::Resolve_CollisionOwner(GameObject* notifyOwner) const
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        const wstring tag = String::ToWString(_colliderTag);
        if (current->Get_Component<ColliderCom>(tag))
            return current;

        current = current->Get_Owner();
    }

    return nullptr;
}

Shared<ColliderCom> ANS_CollisionEnable::Resolve_Collider(const Shared<GameObject>& owner) const
{
    if (owner == nullptr)
        return nullptr;

    return owner->Get_Component<ColliderCom>(String::ToWString(_colliderTag));
}

void ANS_CollisionEnable::Sync_Collider(const Shared<GameObject>& owner, const Shared<ColliderCom>& collider) const
{
    if (owner == nullptr || owner->Get_Transform() == nullptr || collider == nullptr)
        return;

    collider->Update_Collider(owner->Get_Transform()->Get_WorldMatrix());

    GAME->Add_Collider(collider);
}

void ANS_CollisionEnable::Set_ColliderActive(const AnimNotifyContext& context, bool active) const
{
    if (context.isPreview)
        return;

    const Shared<GameObject> owner = Resolve_CollisionOwner(context.owner);
    const Shared<ColliderCom> collider = Resolve_Collider(owner);

    if (owner == nullptr || collider == nullptr)
        return;

    if (const Shared<SL_Base> slOwner = dynamic_pointer_cast<SL_Base>(owner))
    {
        if (active)
        {
            slOwner->Set_AttackHitAnimOverride(_colliderTag, _hitAnimOverride);
            slOwner->Set_AttackHitEffect(_colliderTag, _hitEffectName, _hitEffectLocalOffset, _hitEffectScale);
        }
        else
        {
            slOwner->Clear_AttackHitAnimOverride(_colliderTag);
            slOwner->Clear_AttackHitEffect(_colliderTag);
        }
    }

    collider->Clear_Overlap();

    if (active)
    {
        GAME->Unregister_PhysXCollider(collider.get());
        collider->Set_IsActive(true);
        Sync_Collider(owner, collider);
        return;
    }

    collider->Set_IsActive(false);
    GAME->Unregister_PhysXCollider(collider.get());
}
