#include "Trigger_Interaction.h"
#include "Client_Constants.h"
#include "ColliderCom.h"
#include "Bounding_AABB.h"
#include "ClientInstance.h"
#include "Monster_WorldBase.h"
#include "NPC_Base.h"
#include "Player_WorldBase.h"

IMPLEMENT_REFLECTION(Trigger_Interaction)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Trigger_Interaction";
    info.category = "Trigger";

    return true;
}

Trigger_Interaction::Trigger_Interaction(const ComPtr<Device>& device, const ComPtr<Context>& context) : GameObject(device, context)
{
}

Trigger_Interaction::Trigger_Interaction(const Trigger_Interaction& prototype) : GameObject(prototype)
{
}

HRESULT Trigger_Interaction::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Trigger_Interaction::Initialize(void* arg)
{
    const auto* triggerDesc = static_cast<TRIGGER_INTERACTION_DESC*>(arg);
    if (triggerDesc == nullptr || triggerDesc->ownerNpc == nullptr)
        return E_FAIL;

    _ownerNPC = triggerDesc->ownerNpc;
    _localOffset = triggerDesc->localOffset;
    _playerOverlapColliders.clear();

    GAMEOBJECT_DESC desc{};
    desc.speedPerSec = 0.f;
    desc.degreePerSec = 0.f;

    CHECK_FAILED(__super::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    Shared<TransformCom> ownerTransform = _ownerNPC->Get_Transform();
    if (ownerTransform == nullptr)
        return E_FAIL;

    Sync_ToOwnerTransform();

    return S_OK;
}

void Trigger_Interaction::Update(float timeDelta)
{
    GameObject::Update(timeDelta);

    if (_ownerNPC == nullptr)
    {
        if (_activeHeroOverlapRegistered)
        {
            _activeHeroOverlapRegistered = false;
            CLIENT->RemoveDialogueInteractionOverlap();
        }

        CLIENT->Clear_InteractionPrompt(this);
        return;
    }

    Sync_ToOwnerTransform();
    const bool hasActiveHeroOverlap = Refresh_ActiveHeroOverlapRegistration();

    if (CLIENT->Is_WorldControlSuspended())
    {
        CLIENT->Clear_InteractionPrompt(this);
        return;
    }

    if (GAME->Is_WorldGameplayInputBlocked())
    {
        CLIENT->Clear_InteractionPrompt(this);
        return;
    }

    if (CLIENT->IsDialogueActive() || CLIENT->IsDialogueInteractLocked())
    {
        CLIENT->Clear_InteractionPrompt(this);
        return;
    }

    if (!hasActiveHeroOverlap)
    {
        CLIENT->Clear_InteractionPrompt(this);
        return;
    }

    CLIENT->Request_InteractionPrompt(this, InteractionPromptType::Dialogue);

    if (GAME->KeyDown(KEY_TYPE::E))
    {
        CLIENT->Clear_InteractionPrompt(this);
        _ownerNPC->BuildDialogueRequest(_ownerNPC->Get_SourcePrefabName());
    }
}

void Trigger_Interaction::Late_Update(float timeDelta)
{
    GameObject::Late_Update(timeDelta);

    if (_colliderCom == nullptr || !_colliderCom->Get_IsActive())
        return;

    _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
    GAME->Add_Collider(_colliderCom);
}

void Trigger_Interaction::OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnStayOverlap(self, other);
}

void Trigger_Interaction::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnBeginOverlap(self, other);

    if (_colliderCom == nullptr || self.get() != _colliderCom.get())
        return;

    const Shared<GameObject> otherOwner = other ? other->Get_Owner() : nullptr;
    if (dynamic_pointer_cast<Player_WorldBase>(otherOwner) == nullptr)
        return;

    _playerOverlapColliders.insert(other.get());

    Refresh_ActiveHeroOverlapRegistration();
}

void Trigger_Interaction::OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnEndOverlap(self, other);

    if (_colliderCom == nullptr || self.get() != _colliderCom.get())
        return;

    const Shared<GameObject> otherOwner = other ? other->Get_Owner() : nullptr;
    if (dynamic_pointer_cast<Player_WorldBase>(otherOwner) == nullptr)
        return;

    if (other == nullptr)
        return;

    _playerOverlapColliders.erase(other.get());

    if (!Refresh_ActiveHeroOverlapRegistration())
        CLIENT->Clear_InteractionPrompt(this);
}

void Trigger_Interaction::Sync_ToOwnerTransform()
{
    if (_ownerNPC == nullptr || _transformCom == nullptr)
        return;

    Shared<TransformCom> ownerTransform = _ownerNPC->Get_Transform();
    if (ownerTransform == nullptr)
        return;

    Vec3 ownerPos = ownerTransform->Get_WorldPosition();
    Vec3 ownerForward = ownerTransform->Get_WorldForward();
    ownerForward.y = 0.f;

    if (ownerForward.LengthSquared() > 0.0001f)
        ownerForward.Normalize();
    else
        ownerForward = Vec3::Look;

    Vec3 ownerRight = ownerTransform->Get_WorldRight();
    ownerRight.y = 0.f;

    if (ownerRight.LengthSquared() > 0.0001f)
        ownerRight.Normalize();
    else
        ownerRight = Vec3::Right;

    const Vec3 triggerPos =
        ownerPos +
        ownerRight * _localOffset.x +
        Vec3::Up * _localOffset.y +
        ownerForward * _localOffset.z;

    _transformCom->Set_WorldPosition(triggerPos);
    _transformCom->Set_WorldRotationEuler(ownerTransform->Get_WorldRotationEuler());
}

HRESULT Trigger_Interaction::Ready_Components()
{
    Bounding_AABB::BoundingAABBDesc aabbDesc{};

    aabbDesc.extents = Vec3(3.f, 3.5f, 3.f);
    aabbDesc.center = Vec3(0.f, 0.2f, 0.f);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_AABB"), _colliderCom, &aabbDesc), E_FAIL);
    _colliderCom->Set_CollisionPreset(Collision_Preset::Trigger);
    _colliderCom->Set_IsActive(true);

    return S_OK;
}

bool Trigger_Interaction::Has_ActiveHeroOverlap() const
{
    for (ColliderCom* collider : _playerOverlapColliders)
    {
        if (collider == nullptr)
            continue;

        if (CLIENT->Is_ActiveHero(collider->Get_Owner()))
            return true;
    }

    return false;
}

bool Trigger_Interaction::Refresh_ActiveHeroOverlapRegistration()
{
    const bool hasActiveHeroOverlap = Has_ActiveHeroOverlap();
    if (_activeHeroOverlapRegistered == hasActiveHeroOverlap)
        return hasActiveHeroOverlap;

    _activeHeroOverlapRegistered = hasActiveHeroOverlap;
    if (_activeHeroOverlapRegistered)
        CLIENT->AddDialogueInteractionOverlap();
    else
        CLIENT->RemoveDialogueInteractionOverlap();

    return hasActiveHeroOverlap;
}

Shared<Trigger_Interaction> Trigger_Interaction::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Trigger_Interaction>(device, context);

    if(FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : Trigger_Interaction");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Trigger_Interaction::Clone(void* arg)
{
    auto clone = make_shared<Trigger_Interaction>(*this);

    if(FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : Trigger_Interaction");
        return nullptr;
    }
    return clone;
}

void Trigger_Interaction::Free()
{
    if (_activeHeroOverlapRegistered)
    {
        _activeHeroOverlapRegistered = false;
        CLIENT->RemoveDialogueInteractionOverlap();
    }

    _playerOverlapColliders.clear();
    CLIENT->Clear_InteractionPrompt(this);

    GameObject::Free();
}
