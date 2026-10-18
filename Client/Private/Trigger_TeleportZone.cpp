#include "Trigger_TeleportZone.h"

#include "Bounding_AABB.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "MovementCom.h"

IMPLEMENT_REFLECTION(Trigger_TeleportZone)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Trigger_TeleportZone";
    info.category = "Trigger";

    using SelfType = Trigger_TeleportZone;
    PROPERTY_VEC3("Teleport Target Position", _teleportTargetPosition);
    PROPERTY_VEC3("Teleport Target Rotation", _teleportTargetRotation);

    return true;
}

Trigger_TeleportZone::Trigger_TeleportZone(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

Trigger_TeleportZone::Trigger_TeleportZone(const Trigger_TeleportZone& prototype)
    : GameObject(prototype)
{
}

HRESULT Trigger_TeleportZone::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Trigger_TeleportZone::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    _hasTriggered = false;

    return S_OK;
}

void Trigger_TeleportZone::Late_Update(float timeDelta)
{
    GameObject::Late_Update(timeDelta);

    if (!_colliderCom || !_colliderCom->Get_IsActive())
    {
        return;
    }

    _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
    GAME->Add_Collider(_colliderCom);
}

void Trigger_TeleportZone::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnBeginOverlap(self, other);

    if (self.get() != _colliderCom.get())
    {
        return;
    }

    if (!Is_ActivePlayerCollider(other))
    {
        return;
    }

    if (_hasTriggered)
    {
        return;
    }

    _hasTriggered = true;

    const Shared<GameObject> player = other->Get_Owner();
    const Vec3 targetPosition = _teleportTargetPosition;

    const bool requested = CLIENT->Request_ScreenFadeTransition(
        [this, player, targetPosition]()
        {
            if (player)
            {
                player->Get_Transform()->Set_WorldPosition(targetPosition);
                player->Get_Transform()->Set_WorldRotationEuler(_teleportTargetRotation);
                

                if (const Shared<MovementCom> movement = player->Get_Component<MovementCom>())
                {
                    movement->Stop_VerticalMovement();
                    movement->Sync_RuntimePositionToTransform();
                }
            }

            _hasTriggered = false;
        });

    if (!requested)
    {
        _hasTriggered = false;

        return;
    }

    GAME->Stop_SoundChannel(SoundChannel::BGM, 0.3f);
}

void Trigger_TeleportZone::OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnEndOverlap(self, other);
}

void Trigger_TeleportZone::BeginPlay()
{
    GameObject::BeginPlay();
}

HRESULT Trigger_TeleportZone::Ready_Components()
{
    Bounding_AABB::BoundingAABBDesc aabbDesc{};
    aabbDesc.center = Vec3(0.f, 1.f, 0.f);
    aabbDesc.extents = Vec3(3.f, 2.f, 3.f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_AABB"), _colliderCom, &aabbDesc), E_FAIL);
    _colliderCom->Set_CollisionPreset(Collision_Preset::Trigger);
    _colliderCom->Set_IsActive(true);

    return S_OK;
}

bool Trigger_TeleportZone::Is_ActivePlayerCollider(const Shared<ColliderCom>& other) const
{
    const Shared<GameObject> overlapOwner = other ? other->Get_Owner() : nullptr;
    if (!overlapOwner)
    {
        return false;
    }


    Shared<GameObject> heroOwner = overlapOwner;
    if (!CLIENT->Is_ActiveHero(heroOwner))
    {
        heroOwner = overlapOwner->Get_Owner();
    }

    return CLIENT->Is_ActiveHero(heroOwner);
}

Shared<Trigger_TeleportZone> Trigger_TeleportZone::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Trigger_TeleportZone>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : Trigger_TeleportZone");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Trigger_TeleportZone::Clone(void* arg)
{
    auto clone = make_shared<Trigger_TeleportZone>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : Trigger_TeleportZone");
        return nullptr;
    }

    return clone;
}

void Trigger_TeleportZone::Free()
{
    GameObject::Free();
}
