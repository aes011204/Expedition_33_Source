#include "WorldGrapplingRopeSpot.h"

#include "Bounding_AABB.h"
#include "Bounding_Sphere.h"
#include "Camera.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "Material.h"
#include "ModelCom.h"
#include "Player_WorldBase.h"
#include "ShaderCom.h"
#include "TargetCamera.h"
#include "TransformCom.h"

namespace
{
    constexpr const wchar_t* kGateColliderTag = L"Com_RopeGateCollider";
    constexpr const wchar_t* kGrapplePointColliderTag = L"Com_RopeGrapplePointCollider";
    constexpr const wchar_t* kLandingDebugColliderTag = L"Com_GrappleLandingDebugCollider";
    constexpr const wchar_t* kGrappleModelComponentTag = L"Com_GrappleModel";
    constexpr const wchar_t* kDefaultGrappleModelPath = L"Models/World/Grapple/Grapple.model";

    bool NearlyEqual(const Vec3& lhs, const Vec3& rhs)
    {
        constexpr float kEpsilon = 0.0001f;
        return fabsf(lhs.x - rhs.x) <= kEpsilon &&
            fabsf(lhs.y - rhs.y) <= kEpsilon &&
            fabsf(lhs.z - rhs.z) <= kEpsilon;
    }

    bool NearlyEqual(float lhs, float rhs)
    {
        constexpr float kEpsilon = 0.0001f;
        return fabsf(lhs - rhs) <= kEpsilon;
    }
}

IMPLEMENT_REFLECTION(WorldGrapplingRopeSpot)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "WorldRope";
    info.category = "Trigger";

    PROPERTY_INT("Priority", _priority);
    PROPERTY_WSTRING("Model Path", _modelPath);

    PROPERTY_FLOAT("Max Lock Ray Distance", _maxLockRayDistance, 0.1f, 100.f, 0.1f);

    PROPERTY_VEC3("Start Point Offset", _startPointOffset);
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
    _priority = 0;
    _promptText = L"Rope";
}

WorldGrapplingRopeSpot::WorldGrapplingRopeSpot(const WorldGrapplingRopeSpot& prototype)
    : WorldInteractionSpot(prototype)
    , _modelPath(prototype._modelPath)
    , _gateColliderCenter(prototype._gateColliderCenter)
    , _gateColliderExtents(prototype._gateColliderExtents)
    , _grapplePointColliderCenter(prototype._grapplePointColliderCenter)
    , _grapplePointColliderRadius(prototype._grapplePointColliderRadius)
    , _landingDebugColliderCenter(prototype._landingDebugColliderCenter)
    , _landingDebugColliderRadius(prototype._landingDebugColliderRadius)
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
    Sync_RuntimeTuning();

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

    if (_landingDebugCollider != nullptr && _landingDebugCollider->Get_IsActive())
    {
        _landingDebugCollider->Update_Collider(_transformCom->Get_WorldMatrix());
        GAME->Add_Collider(_landingDebugCollider);
    }

    if (!Is_Visible() || _model == nullptr || _shader == nullptr)
        return;

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());

    if (Casts_DynamicShadow())
        GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
}

HRESULT WorldGrapplingRopeSpot::Render()
{
    if (_model == nullptr || _shader == nullptr)
        return S_OK;

    CHECK_FAILED(Bind_ShaderResources(), E_FAIL);
    CHECK_FAILED(_shader->Reset_DrawFeatures(), E_FAIL);

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
    {
        const uint32 materialIndex = _model->Get_MeshMaterialIndex(meshIndex);
        const Shared<Material> material = _model->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        if (FAILED(_shader->Bind_MaterialTextures(*material)))
            continue;

        CHECK_FAILED(_shader->Bind_CBufferData(material->Get_CB()), E_FAIL);
        CHECK_FAILED(_shader->Begin(material->Get_PassIndex()), E_FAIL);
        CHECK_FAILED(_model->Render(meshIndex), E_FAIL);
    }

    return S_OK;
}

HRESULT WorldGrapplingRopeSpot::Render_Shadow()
{
    if (_model == nullptr || _shader == nullptr || _transformCom == nullptr)
        return S_OK;

    CHECK_FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix()), E_FAIL);
    CHECK_FAILED(GAME->Bind_ShadowCB(_shader), E_FAIL);

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
    {
        CHECK_FAILED(_shader->Begin(1), E_FAIL);
        CHECK_FAILED(_model->Render(meshIndex), E_FAIL);
    }

    return S_OK;
}

bool WorldGrapplingRopeSpot::Try_Get_ShadowWorldBounds(BoundingBox& outWorldBounds) const
{
    if (_model == nullptr || _transformCom == nullptr)
        return false;

    Vec3 localMin{}, localMax{};
    if (!_model->Try_Get_LocalBounds(localMin, localMax))
        return false;

    BoundingBox localBox{};
    BoundingBox::CreateFromPoints(localBox, XMLoadFloat3(&localMin), XMLoadFloat3(&localMax));
    localBox.Transform(outWorldBounds, _model->Get_PreLocalTransformMatrix() * _transformCom->Get_WorldMatrix());

    return true;
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

    if (activeHero == nullptr)
        return;

    _hasActiveHeroOverlap = false;
    activeHero->Clear_WorldInteractionCandidate(this);
    activeHero->Clear_WorldInteractionRequest(this);
}

bool WorldGrapplingRopeSpot::Can_Interact(const Shared<Player_WorldBase>& activeHero) const
{
    if (!__super::Can_Interact(activeHero))
        return false;

    return Is_ActiveHeroInsideGate(activeHero) && Is_CameraLockRayHittingGrapplePoint();
}

bool WorldGrapplingRopeSpot::Build_InteractionRequest(const Shared<Player_WorldBase>& activeHero, WorldInteractionRequest& outRequest) const
{
    if (!Can_Interact(activeHero))
        return false;

    outRequest = {};
    outRequest.type = WorldInteractionType::RopeLook;
    outRequest.ropeDirection = RopeDirection::Launch;
    Build_CommonRequest(outRequest);
    outRequest.targetPoint = Resolve_ColliderWorldCenter(_landingDebugCollider, _targetPointOffset);

    return true;
}

InteractionPromptType WorldGrapplingRopeSpot::Get_InteractionPromptType(const Shared<Player_WorldBase>& activeHero) const
{
    return Can_Interact(activeHero) ? InteractionPromptType::Cross : InteractionPromptType::None;
}

HRESULT WorldGrapplingRopeSpot::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxMesh"), _shader), E_FAIL);
    CHECK_FAILED(Ready_ModelComponent(), E_FAIL);
    CHECK_FAILED(Ready_AABBCollider(kGateColliderTag, _gateColliderCenter, _gateColliderExtents, _gateCollider), E_FAIL);
    CHECK_FAILED(Ready_SphereCollider(kGrapplePointColliderTag, _grapplePointColliderCenter, _grapplePointColliderRadius, _grapplePointCollider), E_FAIL);
    CHECK_FAILED(Ready_DebugSphereCollider(kLandingDebugColliderTag, _landingDebugColliderCenter, _landingDebugColliderRadius, _landingDebugCollider), E_FAIL);

    return S_OK;
}

HRESULT WorldGrapplingRopeSpot::Ready_ModelComponent()
{
    const std::filesystem::path relativeModelPath =
        _modelPath.empty() ? std::filesystem::path(kDefaultGrappleModelPath) : std::filesystem::path(_modelPath);
    const std::filesystem::path modelPath =
        relativeModelPath.is_absolute() ? relativeModelPath : std::filesystem::path(GAME->Get_AssetRoot()) / relativeModelPath;

    _model = ModelCom::Create(
        _device,
        _context,
        ModelType::NonAnim,
        String::ToString(modelPath.wstring()).c_str(),
        Matrix::CreateScale(0.001f));

    CHECK_NULL(_model, E_FAIL);

    Register_ModelComponent();

    return S_OK;
}

void WorldGrapplingRopeSpot::Register_ModelComponent()
{
    if (_model == nullptr)
        return;

    _model->Set_Owner(GetSharedPtr<GameObject>());

    if (_components.find(kGrappleModelComponentTag) == _components.end())
        _components.emplace(kGrappleModelComponentTag, static_pointer_cast<Component>(_model));
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

HRESULT WorldGrapplingRopeSpot::Ready_SphereCollider(
    const wstring& componentTag,
    const Vec3& center,
    float radius,
    Shared<ColliderCom>& outCollider)
{
    Bounding_Sphere::FBoundingSphereDesc sphereDesc{};
    sphereDesc.center = center;
    sphereDesc.radius = std::max(0.01f, radius);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_Sphere"), outCollider, &sphereDesc), E_FAIL);
    CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_Sphere"), componentTag), E_FAIL);

    outCollider->Set_CollisionPreset(Collision_Preset::Trigger);
    outCollider->Set_IsActive(true);

    return S_OK;
}

HRESULT WorldGrapplingRopeSpot::Ready_DebugSphereCollider(
    const wstring& componentTag,
    const Vec3& center,
    float radius,
    Shared<ColliderCom>& outCollider)
{
    Bounding_Sphere::FBoundingSphereDesc sphereDesc{};
    sphereDesc.center = center;
    sphereDesc.radius = std::max(0.01f, radius);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_Sphere"), outCollider, &sphereDesc), E_FAIL);
    CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_Sphere"), componentTag), E_FAIL);

    outCollider->Set_CollisionPreset(Collision_Preset::Custom);
    outCollider->Set_IsActive(true);

    return S_OK;
}

void WorldGrapplingRopeSpot::Sync_RuntimeTuning()
{
    Sync_AABBCollider(
        _gateCollider,
        _gateColliderCenter,
        _gateColliderExtents,
        _lastSyncedGateColliderCenter,
        _lastSyncedGateColliderExtents,
        _hasSyncedGateCollider);

    Sync_SphereCollider(
        _grapplePointCollider,
        _grapplePointColliderCenter,
        _grapplePointColliderRadius,
        _lastSyncedGrapplePointColliderCenter,
        _lastSyncedGrapplePointColliderRadius,
        _hasSyncedGrapplePointCollider);

    Sync_SphereCollider(
        _landingDebugCollider,
        _landingDebugColliderCenter,
        _landingDebugColliderRadius,
        _lastSyncedLandingDebugColliderCenter,
        _lastSyncedLandingDebugColliderRadius,
        _hasSyncedLandingDebugCollider);
}

void WorldGrapplingRopeSpot::Sync_AABBCollider(
    const Shared<ColliderCom>& collider,
    Vec3& center,
    Vec3& extents,
    Vec3& lastSyncedCenter,
    Vec3& lastSyncedExtents,
    bool& hasSynced)
{
    if (collider == nullptr)
        return;

    ColliderShapeDesc desc = collider->Get_ShapeDesc();

    if (!hasSynced)
    {
        center = desc.center;
        extents = desc.extents;
        lastSyncedCenter = desc.center;
        lastSyncedExtents = desc.extents;
        hasSynced = true;
        return;
    }

    const bool colliderChanged =
        !NearlyEqual(desc.center, lastSyncedCenter) ||
        !NearlyEqual(desc.extents, lastSyncedExtents);
    const bool tuningChanged =
        !NearlyEqual(center, lastSyncedCenter) ||
        !NearlyEqual(extents, lastSyncedExtents);

    if (colliderChanged && !tuningChanged)
    {
        center = desc.center;
        extents = desc.extents;
        lastSyncedCenter = desc.center;
        lastSyncedExtents = desc.extents;
        return;
    }

    if (!tuningChanged)
        return;

    desc.center = center;
    desc.extents = extents;
    collider->Set_ShapeDesc(desc);
    lastSyncedCenter = center;
    lastSyncedExtents = extents;
}

void WorldGrapplingRopeSpot::Sync_SphereCollider(
    const Shared<ColliderCom>& collider,
    Vec3& center,
    float& radius,
    Vec3& lastSyncedCenter,
    float& lastSyncedRadius,
    bool& hasSynced)
{
    if (collider == nullptr)
        return;

    ColliderShapeDesc desc = collider->Get_ShapeDesc();
    desc.radius = std::max(0.01f, desc.radius);

    if (!hasSynced)
    {
        center = desc.center;
        radius = desc.radius;
        lastSyncedCenter = desc.center;
        lastSyncedRadius = desc.radius;
        hasSynced = true;
        return;
    }

    radius = std::max(0.01f, radius);

    const bool colliderChanged =
        !NearlyEqual(desc.center, lastSyncedCenter) ||
        !NearlyEqual(desc.radius, lastSyncedRadius);
    const bool tuningChanged =
        !NearlyEqual(center, lastSyncedCenter) ||
        !NearlyEqual(radius, lastSyncedRadius);

    if (colliderChanged && !tuningChanged)
    {
        center = desc.center;
        radius = desc.radius;
        lastSyncedCenter = desc.center;
        lastSyncedRadius = desc.radius;
        return;
    }

    if (!tuningChanged)
        return;

    desc.center = center;
    desc.radius = radius;
    collider->Set_ShapeDesc(desc);
    lastSyncedCenter = center;
    lastSyncedRadius = radius;
}


void WorldGrapplingRopeSpot::Refresh_CandidateFromLockRay(const Shared<Player_WorldBase>& activeHero)
{
    if (activeHero == nullptr || activeHero->Is_WorldInteracting())
        return;

    if (Is_ActiveHeroInsideGate(activeHero) && Is_CameraLockRayHittingGrapplePoint())
        activeHero->Set_WorldInteractionCandidate(GetSharedPtr<GameObject>());
    else
        activeHero->Clear_WorldInteractionCandidate(this);
}

Vec3 WorldGrapplingRopeSpot::Resolve_ColliderWorldCenter(
    const Shared<ColliderCom>& collider,
    const Vec3& fallbackLocalOffset) const
{
    if (_transformCom == nullptr)
        return fallbackLocalOffset;

    Vec3 localCenter = fallbackLocalOffset;
    if (collider != nullptr)
        localCenter = collider->Get_ShapeDesc().center;

    return Vec3::Transform(localCenter, _transformCom->Get_WorldMatrix());
}

bool WorldGrapplingRopeSpot::Is_ActiveHeroInsideGate(const Shared<Player_WorldBase>& activeHero) const
{
    if (activeHero == nullptr || _gateCollider == nullptr)
        return false;

    if (_hasActiveHeroOverlap)
        return true;

    const Shared<ColliderCom> heroCollider = activeHero->Get_Component<ColliderCom>();
    return heroCollider != nullptr && _gateCollider->Intersect(heroCollider);
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
    const Shared<Bounding_Sphere> sphere = dynamic_pointer_cast<Bounding_Sphere>(bounding);
    if (sphere == nullptr)
        return false;

    float hitDistance = 0.f;
    if (!sphere->Get_Sphere().Intersects(XMLoadFloat3(&rayOrigin), XMLoadFloat3(&rayDirection), hitDistance))
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

HRESULT WorldGrapplingRopeSpot::Bind_ShaderResources()
{
    CHECK_FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix()), E_FAIL);
    CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);

    return S_OK;
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
