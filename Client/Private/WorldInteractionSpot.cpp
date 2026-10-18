#include "WorldInteractionSpot.h"

#include "ClientInstance.h"
#include "ColliderCom.h"
#include "Player_WorldBase.h"
#include "TransformCom.h"

WorldInteractionSpot::WorldInteractionSpot(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

WorldInteractionSpot::WorldInteractionSpot(const WorldInteractionSpot& prototype)
    : GameObject(prototype)
    , _startPointOffset(prototype._startPointOffset)
    , _targetPointOffset(prototype._targetPointOffset)
    , _wallPointOffset(prototype._wallPointOffset)
    , _ropePointOffset(prototype._ropePointOffset)
    , _faceDirection(prototype._faceDirection)
    , _warpEndProgress(prototype._warpEndProgress)
    , _priority(prototype._priority)
    , _promptText(prototype._promptText)
{
}

HRESULT WorldInteractionSpot::Initialize_Prototype()
{
    return S_OK;
}

HRESULT WorldInteractionSpot::Initialize(void* arg)
{
    GAMEOBJECT_DESC desc{};
    desc.speedPerSec = 0.f;
    desc.degreePerSec = 0.f;

    CHECK_FAILED(__super::Initialize(&desc), E_FAIL);

    return S_OK;
}

bool WorldInteractionSpot::Can_Interact(const Shared<Player_WorldBase>& activeHero) const
{
    if (activeHero == nullptr || activeHero->Is_WorldInteracting())
        return false;

    return activeHero->Get_WorldInteractionCandidate().get() == this;
}

void WorldInteractionSpot::Build_CommonRequest(WorldInteractionRequest& outRequest) const
{
    outRequest.startPoint = Resolve_WorldPoint(_startPointOffset);
    outRequest.targetPoint = Resolve_WorldPoint(_targetPointOffset);
    outRequest.wallPoint = Resolve_WorldPoint(_wallPointOffset);
    outRequest.ropePoint = Resolve_WorldPoint(_ropePointOffset);
    outRequest.faceDir = Resolve_WorldDirection(_faceDirection);
    outRequest.warpEndProgress = std::clamp(_warpEndProgress, 0.01f, 1.f);
}

Vec3 WorldInteractionSpot::Resolve_WorldPoint(const Vec3& localOffset) const
{
    if (_transformCom == nullptr)
        return localOffset;

    return Vec3::Transform(localOffset, _transformCom->Get_WorldMatrix());
}

Vec3 WorldInteractionSpot::Resolve_WorldDirection(const Vec3& localDirection) const
{
    Vec3 direction = localDirection;
    if (direction.LengthSquared() <= 0.0001f)
        direction = Vec3::Look;

    if (_transformCom != nullptr)
        direction = Vec3::TransformNormal(direction, _transformCom->Get_WorldMatrix());

    direction.y = 0.f;
    if (direction.LengthSquared() <= 0.0001f)
        direction = Vec3::Look;

    direction.Normalize();
    return direction;
}

void WorldInteractionSpot::Register_CandidateIfActiveHero(const Shared<ColliderCom>& other)
{
    Shared<Player_WorldBase> activeHero{};
    if (!Is_ActiveHeroCollider(other, &activeHero))
        return;

    if (activeHero != nullptr && !activeHero->Is_WorldInteracting())
        activeHero->Set_WorldInteractionCandidate(GetSharedPtr<GameObject>());
}

bool WorldInteractionSpot::Is_ActiveHeroCollider(const Shared<ColliderCom>& other, Shared<Player_WorldBase>* outActiveHero) const
{
    if (outActiveHero != nullptr)
        *outActiveHero = nullptr;

    const Shared<GameObject> otherOwner = other ? other->Get_Owner() : nullptr;
    const Shared<Player_WorldBase> activeHero = CLIENT->Get_ActiveHero();

    if (activeHero == nullptr || otherOwner != static_pointer_cast<GameObject>(activeHero))
        return false;

    if (outActiveHero != nullptr)
        *outActiveHero = activeHero;

    return true;
}
