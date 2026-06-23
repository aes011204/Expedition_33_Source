#include "ANS_SL_PartCollisionEnable.h"

#include "ColliderCom.h"
#include "ContainerObject.h"
#include "GameInstance.h"
#include "PartObject.h"
#include "SL_Base.h"
#include "SL_Dualliste.h"

IMPLEMENT_REFLECTION(ANS_SL_PartCollisionEnable)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ANS_SL_PartCollisionEnable";
    info.category = "SoulLike";

    PROPERTY_STRING("Part Tag", _partTag);
    PROPERTY_STRING("Collider Tag", _colliderTag);
    PROPERTY_ENUM("Hit Reaction", _hitAnimOverride, SL_HitAnimOverride);

    return true;
}

void ANS_SL_PartCollisionEnable::On_Begin(const AnimNotifyContext& context)
{
    Set_ColliderActive(context, true);
}

void ANS_SL_PartCollisionEnable::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_SL_PartCollisionEnable::On_End(const AnimNotifyContext& context)
{
    Set_ColliderActive(context, false);
}

Shared<ContainerObject> ANS_SL_PartCollisionEnable::Resolve_OwnerContainer(GameObject* notifyOwner) const
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(current))
            return ownerContainer;

        current = current->Get_Owner();
    }

    return nullptr;
}

Shared<PartObject> ANS_SL_PartCollisionEnable::Resolve_Part(const Shared<ContainerObject>& ownerContainer) const
{
    if (ownerContainer == nullptr)
        return nullptr;

    PartObject* part = ownerContainer->Get_PartObject(String::ToWString(_partTag));
    if (part == nullptr)
        return nullptr;

    return part->GetSharedPtr<PartObject>();
}

Shared<ColliderCom> ANS_SL_PartCollisionEnable::Resolve_Collider(const Shared<PartObject>& part) const
{
    if (part == nullptr)
        return nullptr;

    return part->Get_Component<ColliderCom>(String::ToWString(_colliderTag));
}

void ANS_SL_PartCollisionEnable::Set_ColliderActive(const AnimNotifyContext& context, bool active) const
{
    if (context.isPreview)
        return;

    const Shared<ContainerObject> ownerContainer = Resolve_OwnerContainer(context.owner);
    const Shared<PartObject> part = Resolve_Part(ownerContainer);
    const Shared<ColliderCom> collider = Resolve_Collider(part);

    if (collider == nullptr)
        return;

    const Shared<SL_Base> slOwner = dynamic_pointer_cast<SL_Base>(ownerContainer);

    if (const Shared<SL_Dualliste> dualliste = dynamic_pointer_cast<SL_Dualliste>(ownerContainer))
    {
        if (_partTag == "Part_Sword_L" && !dualliste->CanUseLeftSword())
        {
            if (slOwner)
                slOwner->Clear_AttackHitAnimOverride(_colliderTag);

            collider->Set_IsActive(false);
            collider->Clear_Overlap();
            GAME->Unregister_PhysXCollider(collider.get());
            return;
        }
    }

    if (slOwner)
    {
        if (active)
            slOwner->Set_AttackHitAnimOverride(_colliderTag, _hitAnimOverride);
        else
            slOwner->Clear_AttackHitAnimOverride(_colliderTag);
    }

    collider->Clear_Overlap();

    if (active)
    {
        GAME->Unregister_PhysXCollider(collider.get());
        collider->Set_IsActive(true);
        return;
    }

    collider->Set_IsActive(false);
    GAME->Unregister_PhysXCollider(collider.get());
}
