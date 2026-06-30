#include "WorldLedgeSpot.h"

#include "Bounding_AABB.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "Player_WorldBase.h"

IMPLEMENT_REFLECTION(WorldLedgeSpot)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "WorldLedge";
    info.category = "Trigger";

    PROPERTY_INT("Priority", _priority);
    PROPERTY_VEC3("Collider Center", _colliderCenter);
    PROPERTY_VEC3("Collider Extents", _colliderExtents);
    PROPERTY_VEC3("Start Point Offset", _startPointOffset);
    PROPERTY_VEC3("Target Point Offset", _targetPointOffset);
    PROPERTY_VEC3("Wall Point Offset", _wallPointOffset);
    PROPERTY_VEC3("Face Direction", _faceDirection);
    PROPERTY_FLOAT("Warp End Progress", _warpEndProgress, 0.01f, 1.f, 0.01f);
    PROPERTY_WSTRING("Prompt Text", _promptText);

    return true;
}

WorldLedgeSpot::WorldLedgeSpot(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : WorldInteractionSpot(device, context)
{
    _promptText = L"Ledge";
}

WorldLedgeSpot::WorldLedgeSpot(const WorldLedgeSpot& prototype)
    : WorldInteractionSpot(prototype)
    , _colliderCenter(prototype._colliderCenter)
    , _colliderExtents(prototype._colliderExtents)
{
}

HRESULT WorldLedgeSpot::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    return S_OK;
}

void WorldLedgeSpot::Late_Update(float timeDelta)
{
    __super::Late_Update(timeDelta);

    if (_colliderCom == nullptr || !_colliderCom->Get_IsActive())
        return;

    _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
    GAME->Add_Collider(_colliderCom);
}

void WorldLedgeSpot::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    __super::OnBeginOverlap(self, other);

    if (self.get() != _colliderCom.get())
        return;

    Register_CandidateIfActiveHero(other);
}

void WorldLedgeSpot::OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    __super::OnStayOverlap(self, other);

    if (self.get() != _colliderCom.get())
        return;

    Register_CandidateIfActiveHero(other);
}

void WorldLedgeSpot::OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    __super::OnEndOverlap(self, other);

    if (self.get() != _colliderCom.get())
        return;

    Shared<Player_WorldBase> activeHero{};
    if (!Is_ActiveHeroCollider(other, &activeHero))
        return;

    if (activeHero != nullptr && !activeHero->Is_WorldInteracting())
        activeHero->Clear_WorldInteractionCandidate(this);
}

bool WorldLedgeSpot::Build_InteractionRequest(const Shared<Player_WorldBase>& activeHero, WorldInteractionRequest& outRequest) const
{
    if (!Can_Interact(activeHero))
        return false;

    outRequest = {};
    outRequest.type = WorldInteractionType::WallClimb;
    Build_CommonRequest(outRequest);

    return true;
}

HRESULT WorldLedgeSpot::Ready_Components()
{
    Bounding_AABB::BoundingAABBDesc aabbDesc{};
    aabbDesc.center = _colliderCenter;
    aabbDesc.extents = _colliderExtents;

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_AABB"), _colliderCom, &aabbDesc), E_FAIL);
    _colliderCom->Set_CollisionPreset(Collision_Preset::Trigger);
    _colliderCom->Set_IsActive(true);

    return S_OK;
}

Shared<WorldLedgeSpot> WorldLedgeSpot::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<WorldLedgeSpot>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : WorldLedge");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> WorldLedgeSpot::Clone(void* arg)
{
    auto clone = make_shared<WorldLedgeSpot>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : WorldLedge");
        return nullptr;
    }

    return clone;
}

void WorldLedgeSpot::Free()
{
    WorldInteractionSpot::Free();
}
