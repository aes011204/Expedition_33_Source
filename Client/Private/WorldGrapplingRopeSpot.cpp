#include "WorldGrapplingRopeSpot.h"

#include "Bounding_AABB.h"
#include "Camera.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "Player_WorldBase.h"
#include "TargetCamera.h"
#include "TransformCom.h"

namespace
{
    constexpr const wchar_t* kGateColliderTag = L"Com_RopeGateCollider";
    constexpr const wchar_t* kGrapplePointColliderTag = L"Com_RopeGrapplePointCollider";
}

IMPLEMENT_REFLECTION(WorldGrapplingRopeSpot)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "WorldRope";
    info.category = "Trigger";

    PROPERTY_INT("Priority", _priority);

    PROPERTY_VEC3("Gate Collider Center", _gateColliderCenter);
    PROPERTY_VEC3("Gate Collider Extents", _gateColliderExtents);

    PROPERTY_VEC3("Grapple Point Collider Center", _grapplePointColliderCenter);
    PROPERTY_VEC3("Grapple Point Collider Extents", _grapplePointColliderExtents);
    PROPERTY_FLOAT("Max Lock Ray Distance", _maxLockRayDistance, 0.1f, 100.f, 0.1f);

    PROPERTY_VEC3("Start Point Offset", _startPointOffset);
    PROPERTY_VEC3("Target Point Offset", _targetPointOffset);
    PROPERTY_VEC3("Wall Point Offset", _wallPointOffset);
    PROPERTY_VEC3("Rope Point Offset", _ropePointOffset);
    PROPERTY_VEC3("Face Direction", _faceDirection);
    PROPERTY_FLOAT("Warp End Progress", _warpEndProgress, 0.01f, 1.f, 0.01f);
    PROPERTY_WSTRING("Prompt Text", _promptText);

    return true;
}

WorldGrapplingRopeSpot::WorldGrapplingRopeSpot(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : WorldInteractionSpot(device, context)
{
    _startPointOffset = Vec3(0.f, 0.f, 0.f);
    _targetPointOffset = Vec3(0.f, 3.f, 0.f);
    _ropePointOffset = Vec3(0.f, 3.f, 0.f);
    _warpEndProgress = 0.95f;
    _promptText = L"Rope";
}

WorldGrapplingRopeSpot::WorldGrapplingRopeSpot(const WorldGrapplingRopeSpot& prototype)
    : WorldInteractionSpot(prototype)
    , _gateColliderCenter(prototype._gateColliderCenter)
    , _gateColliderExtents(prototype._gateColliderExtents)
    , _grapplePointColliderCenter(prototype._grapplePointColliderCenter)
    , _grapplePointColliderExtents(prototype._grapplePointColliderExtents)
    , _maxLockRayDistance(prototype._maxLockRayDistance)
{
}

HRESULT WorldGrapplingRopeSpot::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    Set_ShadowCasterType(ShadowCasterType::Dynamic);

    return S_OK;
}

void WorldGrapplingRopeSpot::Update(float timeDelta)
{
    __super::Update(timeDelta);

    const Shared<Player_WorldBase> activeHero = CLIENT->Get_ActiveHero();
    Refresh_CandidateFromLockRay(activeHero);
}

void WorldGrapplingRopeSpot::Late_Update(float timeDelta)
{
    __super::Late_Update(timeDelta);

    if (_gateCollider != nullptr && _gateCollider->Get_IsActive())
    {
        _gateCollider->Update_Collider(_transformCom->Get_WorldMatrix());
        GAME->Add_Collider(_gateCollider);
    }

    if (_grapplePointCollider != nullptr && _grapplePointCollider->Get_IsActive())
    {
        _grapplePointCollider->Update_Collider(_transformCom->Get_WorldMatrix());
        GAME->Add_Collider(_grapplePointCollider);
    }
}

HRESULT WorldGrapplingRopeSpot::Render()
{
    return S_OK;
}

HRESULT WorldGrapplingRopeSpot::Render_Shadow()
{
    return S_OK;
}

bool WorldGrapplingRopeSpot::Try_Get_ShadowWorldBounds(BoundingBox& outWorldBounds) const
{
    return false;
}

void WorldGrapplingRopeSpot::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    __super::OnBeginOverlap(self, other);

    if (!Is_GateCollider(self))
        return;

    Shared<Player_WorldBase> activeHero{};
    if (!Is_ActiveHeroCollider(other, &activeHero))
        return;

    _hasActiveHeroOverlap = true;
    Refresh_CandidateFromLockRay(activeHero);
}

void WorldGrapplingRopeSpot::OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    __super::OnStayOverlap(self, other);

    if (!Is_GateCollider(self))
        return;

    Shared<Player_WorldBase> activeHero{};
    if (!Is_ActiveHeroCollider(other, &activeHero))
        return;

    _hasActiveHeroOverlap = true;
    Refresh_CandidateFromLockRay(activeHero);
}

void WorldGrapplingRopeSpot::OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    __super::OnEndOverlap(self, other);

    if (!Is_GateCollider(self))
        return;

    Shared<Player_WorldBase> activeHero{};
    if (!Is_ActiveHeroCollider(other, &activeHero))
        return;

    if (activeHero == nullptr || activeHero->Is_WorldInteracting())
        return;

    _hasActiveHeroOverlap = false;
    activeHero->Clear_WorldInteractionCandidate(this);
}

bool WorldGrapplingRopeSpot::Can_Interact(const Shared<Player_WorldBase>& activeHero) const
{
    if (!__super::Can_Interact(activeHero))
        return false;

    return _hasActiveHeroOverlap && Is_CameraLockRayHittingGrapplePoint();
}

bool WorldGrapplingRopeSpot::Build_InteractionRequest(const Shared<Player_WorldBase>& activeHero, WorldInteractionRequest& outRequest) const
{
    if (!Can_Interact(activeHero))
        return false;

    outRequest = {};
    outRequest.type = WorldInteractionType::RopeLook;
    outRequest.ropeDirection = RopeDirection::Launch;
    Build_CommonRequest(outRequest);

    return true;
}

HRESULT WorldGrapplingRopeSpot::Ready_Components()
{
    CHECK_FAILED(Ready_AABBCollider(kGateColliderTag, _gateColliderCenter, _gateColliderExtents, _gateCollider), E_FAIL);
    CHECK_FAILED(Ready_AABBCollider(kGrapplePointColliderTag, _grapplePointColliderCenter, _grapplePointColliderExtents, _grapplePointCollider), E_FAIL);

    return S_OK;
}

HRESULT WorldGrapplingRopeSpot::Ready_AABBCollider(
    const wstring& componentTag,
    const Vec3& center,
    const Vec3& extents,
    Shared<ColliderCom>& outCollider)
{
    Bounding_AABB::BoundingAABBDesc aabbDesc{};
    aabbDesc.center = center;
    aabbDesc.extents = extents;

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_AABB"), outCollider, &aabbDesc), E_FAIL);
    CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_AABB"), componentTag), E_FAIL);

    outCollider->Set_CollisionPreset(Collision_Preset::Trigger);
    outCollider->Set_IsActive(true);

    return S_OK;
}


void WorldGrapplingRopeSpot::Refresh_CandidateFromLockRay(const Shared<Player_WorldBase>& activeHero)
{
    if (activeHero == nullptr || activeHero->Is_WorldInteracting())
        return;

    if (_hasActiveHeroOverlap && Is_CameraLockRayHittingGrapplePoint())
        activeHero->Set_WorldInteractionCandidate(GetSharedPtr<GameObject>());
    else
        activeHero->Clear_WorldInteractionCandidate(this);
}

bool WorldGrapplingRopeSpot::Is_GateCollider(const Shared<ColliderCom>& collider) const
{
    return collider != nullptr && collider.get() == _gateCollider.get();
}

bool WorldGrapplingRopeSpot::Is_CameraLockRayHittingGrapplePoint() const
{
    if (_grapplePointCollider == nullptr)
        return false;

    Vec3 rayOrigin{};
    Vec3 rayDirection{};
    if (!Try_GetCameraLockRay(rayOrigin, rayDirection))
        return false;

    const Shared<Bounding> bounding = _grapplePointCollider->Get_Bounding();
    const Shared<Bounding_AABB> aabb = dynamic_pointer_cast<Bounding_AABB>(bounding);
    if (aabb == nullptr)
        return false;

    float hitDistance = 0.f;
    if (!aabb->Get_AABB().Intersects(XMLoadFloat3(&rayOrigin), XMLoadFloat3(&rayDirection), hitDistance))
        return false;

    return hitDistance <= _maxLockRayDistance;
}

bool WorldGrapplingRopeSpot::Try_GetCameraLockRay(Vec3& outOrigin, Vec3& outDirection) const
{
    const Shared<Camera> camera = GAME->Get_ActiveCamera();
    if (camera == nullptr)
        return false;

    if (const Shared<TargetCamera> targetCamera = dynamic_pointer_cast<TargetCamera>(camera))
    {
        outOrigin = targetCamera->Get_FreeAimQueryOrigin();
        outDirection = targetCamera->Get_FreeAimQueryDirection();
    }
    else
    {
        if (camera->Get_Transform() == nullptr)
            return false;

        outOrigin = camera->Get_Transform()->Get_WorldPosition();
        outDirection = camera->Get_Transform()->Get_WorldForward();
    }

    if (outDirection.LengthSquared() <= 0.0001f)
        return false;

    outDirection.Normalize();
    return true;
}

Shared<WorldGrapplingRopeSpot> WorldGrapplingRopeSpot::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<WorldGrapplingRopeSpot>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : WorldRope");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> WorldGrapplingRopeSpot::Clone(void* arg)
{
    auto clone = make_shared<WorldGrapplingRopeSpot>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : WorldRope");
        return nullptr;
    }

    return clone;
}

void WorldGrapplingRopeSpot::Free()
{
    WorldInteractionSpot::Free();
}
