#include "WorldRopeSpot.h"

#include "Camera.h"
#include "ClientInstance.h"
#include "GameInstance.h"
#include "Player_WorldBase.h"
#include "TransformCom.h"

IMPLEMENT_REFLECTION(WorldRopeSpot)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "WorldRope";
    info.category = "Trigger";

    PROPERTY_ENUM("Rope Mode", _ropeMode, RopeInteractionMode);
    PROPERTY_ENUM("Rope Direction", _ropeDirection, RopeDirection);
    PROPERTY_INT("Priority", _priority);
    PROPERTY_VEC3("Collider Center", _colliderCenter);
    PROPERTY_VEC3("Collider Extents", _colliderExtents);
    PROPERTY_VEC3("Look Zone Offset", _lookZoneOffset);
    PROPERTY_FLOAT("Look Zone Radius", _lookZoneRadius, 0.01f, 20.f, 0.01f);
    PROPERTY_FLOAT("Max Look Distance", _maxLookDistance, 0.1f, 100.f, 0.1f);
    PROPERTY_VEC3("Start Point Offset", _startPointOffset);
    PROPERTY_VEC3("Target Point Offset", _targetPointOffset);
    PROPERTY_VEC3("Rope Point Offset", _ropePointOffset);
    PROPERTY_VEC3("Face Direction", _faceDirection);
    PROPERTY_FLOAT("Warp End Progress", _warpEndProgress, 0.01f, 1.f, 0.01f);
    PROPERTY_WSTRING("Prompt Text", _promptText);

    return true;
}

WorldRopeSpot::WorldRopeSpot(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : WorldInteractionSpot(device, context)
{
    _targetPointOffset = Vec3(0.f, 0.f, 4.f);
    _warpEndProgress = 0.95f;
    _promptText = L"Rope";
}

WorldRopeSpot::WorldRopeSpot(const WorldRopeSpot& prototype)
    : WorldInteractionSpot(prototype)
    , _ropeMode(prototype._ropeMode)
    , _ropeDirection(prototype._ropeDirection)
    , _lookZoneOffset(prototype._lookZoneOffset)
    , _lookZoneRadius(prototype._lookZoneRadius)
    , _maxLookDistance(prototype._maxLookDistance)
{
}

void WorldRopeSpot::Update(float timeDelta)
{
    WorldInteractionSpot::Update(timeDelta);

    if (_ropeMode != RopeInteractionMode::Look)
        return;

    const Shared<Player_WorldBase> activeHero = CLIENT->Get_ActiveHero();
    if (activeHero == nullptr || activeHero->Is_WorldInteracting())
        return;

    if (Is_CameraLookingAtZone())
        activeHero->Set_WorldInteractionCandidate(GetSharedPtr<GameObject>());
    else
        activeHero->Clear_WorldInteractionCandidate(this);
}

bool WorldRopeSpot::Can_Interact(const Shared<Player_WorldBase>& activeHero) const
{
    if (!WorldInteractionSpot::Can_Interact(activeHero))
        return false;

    if (_ropeMode == RopeInteractionMode::Look)
        return Is_CameraLookingAtZone();

    return true;
}

bool WorldRopeSpot::Build_InteractionRequest(const Shared<Player_WorldBase>& activeHero, WorldInteractionRequest& outRequest) const
{
    if (!Can_Interact(activeHero))
        return false;

    outRequest = {};
    outRequest.type = _ropeMode == RopeInteractionMode::Look
        ? WorldInteractionType::RopeLook
        : WorldInteractionType::RopeTouch;
    outRequest.ropeDirection = _ropeMode == RopeInteractionMode::Look
        ? RopeDirection::Launch
        : _ropeDirection;
    Build_CommonRequest(outRequest);

    return true;
}

bool WorldRopeSpot::Is_CameraLookingAtZone() const
{
    const Shared<Camera> camera = GAME->Get_ActiveCamera();
    if (camera == nullptr || camera->Get_Transform() == nullptr)
        return false;

    const Vec3 rayOrigin = camera->Get_Transform()->Get_WorldPosition();
    Vec3 rayDir = camera->Get_Transform()->Get_WorldForward();
    if (rayDir.LengthSquared() <= 0.0001f)
        return false;
    rayDir.Normalize();

    const Vec3 zoneCenter = Resolve_WorldPoint(_lookZoneOffset);
    const Vec3 toCenter = zoneCenter - rayOrigin;
    const float projected = toCenter.Dot(rayDir);
    if (projected < 0.f || projected > _maxLookDistance)
        return false;

    const Vec3 closestPoint = rayOrigin + rayDir * projected;
    const float radius = max(0.01f, _lookZoneRadius);

    return (zoneCenter - closestPoint).LengthSquared() <= radius * radius;
}

Shared<WorldRopeSpot> WorldRopeSpot::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<WorldRopeSpot>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : WorldRope");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> WorldRopeSpot::Clone(void* arg)
{
    auto clone = make_shared<WorldRopeSpot>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : WorldRope");
        return nullptr;
    }

    return clone;
}

void WorldRopeSpot::Free()
{
    WorldInteractionSpot::Free();
}
