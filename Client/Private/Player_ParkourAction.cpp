#include "pch.h"
#include "Player_ParkourAction.h"

#include "Body_Player.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "MapStaticMesh.h"
#include "ModelCom.h"
#include "PhysX_Types.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"
#include "Player_WorldBase.h"
#include "TransformCom.h"

namespace
{
    constexpr float kRootMotionScale = 0.01f;
    constexpr float kMinParkourHeight = 0.25f;
    constexpr float kMaxParkourHeight = 1.75f;
    constexpr float kLowMantleMaxHeight = 0.95f;
    constexpr float kForwardProbeDistance = 1.25f;
    constexpr float kLandingForwardOffset = 0.45f;
    constexpr float kFallbackWarpEndProgress = 0.90f;

    float SmoothStep01(float value)
    {
        const float t = std::clamp(value, 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
    }

    Vec3 Flatten_Normalized(Vec3 value, const Vec3& fallback)
    {
        value.y = 0.f;
        if (value.LengthSquared() <= 0.0001f)
            value = fallback;

        value.y = 0.f;
        if (value.LengthSquared() <= 0.0001f)
            return Vec3::Look;

        value.Normalize();
        return value;
    }
}

Player_ParkourAction::Player_ParkourAction(const Shared<Player_WorldBase>& owner)
    : Player_State{ owner }
{
}

HRESULT Player_ParkourAction::Initialize()
{
    return Player_State::Initialize();
}

bool Player_ParkourAction::EnterInvoke()
{
    if (!Build_ParkourRequest(_request))
        return false;

    _warpStartPos = _request.startPoint;
    _hasWarp = true;

    if (const Shared<Player_WorldBase> owner = _owner.lock())
    {
        if (const Shared<TransformCom> transform = owner->Get_Transform())
            transform->LookAt(_request.startPoint + _request.faceDir);
    }

    if (_bodyPlayer != nullptr)
    {
        const Body_Player::PlayerWorldAnimation anim =
            (_request.actionType == ParkourActionType::HighMantle)
            ? Body_Player::PlayerWorldAnimation::PL_Parkour_High
            : Body_Player::PlayerWorldAnimation::Pl_Parkour_Low;

        _bodyPlayer->Set_AnimationSpeed(1.f);
        _bodyPlayer->Set_AnimationByState(ETOI(anim), 0.12f, false);

        if (Shared<ModelCom> model = _bodyPlayer->Get_Model())
            model->Set_RootMotionEnabled(true);
    }

    return true;
}

void Player_ParkourAction::Update(float timeDelta)
{
    PlayerControllerCom::RigidControl rigidControl{};
    rigidControl.rigidMovement = true;
    rigidControl.rigidJump = true;
    rigidControl.rigidLook = true;
    _controllerCom->Set_RigidControl(rigidControl);

    const Shared<Player_WorldBase> owner = _owner.lock();
    if (owner == nullptr || !_hasWarp)
    {
        _stateCom->Change_State(PlayerState::Idle);
        return;
    }

    const Shared<TransformCom> transform = owner->Get_Transform();
    if (transform == nullptr)
    {
        _stateCom->Change_State(PlayerState::Idle);
        return;
    }

    const Vec3 currentPos = transform->Get_WorldPosition();
    Vec3 toTarget = _request.targetPoint - currentPos;
    const float remain = toTarget.Length();

    if (remain > 0.01f)
    {
        Vec3 moveDelta = Vec3::Zero;

        if (_bodyPlayer != nullptr)
        {
            if (Shared<ModelCom> model = _bodyPlayer->Get_Model())
            {
                const RootMotionFrame& rm = model->Get_RootTransformVelocity();
                Vec3 localDelta = rm.position * kRootMotionScale;

                if (localDelta.LengthSquared() > 0.000001f)
                {
                    Vec3 rotDeg = transform->Get_WorldRotationEuler();
                    Vec3 rotRad(
                        XMConvertToRadians(rotDeg.x),
                        XMConvertToRadians(rotDeg.y),
                        XMConvertToRadians(rotDeg.z));

                    const Matrix rotMat = XMMatrixRotationRollPitchYaw(rotRad.x, rotRad.y, rotRad.z);
                    moveDelta = Vec3::TransformNormal(localDelta, rotMat);
                }
            }
        }

        if (moveDelta.LengthSquared() <= 0.000001f)
        {
            const float progress = _bodyPlayer ? _bodyPlayer->Get_AnimationProgress() : 1.f;
            const float t = SmoothStep01(progress / kFallbackWarpEndProgress);
            moveDelta = Vec3::Lerp(_warpStartPos, _request.targetPoint, t) - currentPos;
        }

        toTarget = _request.targetPoint - currentPos;
        if (toTarget.LengthSquared() > 0.0001f)
        {
            Vec3 pathDir = toTarget;
            pathDir.Normalize();

            if (moveDelta.Dot(pathDir) < 0.f)
                moveDelta = pathDir * moveDelta.Length();

            if (moveDelta.Length() > remain)
                moveDelta = pathDir * remain;
        }

        transform->Set_WorldPosition(currentPos + moveDelta);
        transform->LookAt(transform->Get_WorldPosition() + _request.faceDir);
    }

    if (_bodyPlayer != nullptr && _bodyPlayer->Is_AnimationFinished())
    {
        transform->Set_WorldPosition(_request.targetPoint);

        if (Has_MoveInput())
            _stateCom->Change_State(PlayerState::Move);
        else
            _stateCom->Change_State(PlayerState::Idle);
    }
}

void Player_ParkourAction::LateUpdate(float timeDelta)
{
}

void Player_ParkourAction::ExitInvoke()
{
    if (_bodyPlayer != nullptr)
    {
        if (Shared<ModelCom> model = _bodyPlayer->Get_Model())
            model->Set_RootMotionEnabled(false);
    }

    if (_controllerCom != nullptr)
        _controllerCom->Set_RigidControl(PlayerControllerCom::RigidControl{});

    if (const Shared<Player_WorldBase> owner = _owner.lock())
        owner->Clear_ParkourCandidate();

    _hasWarp = false;
}

bool Player_ParkourAction::Build_ParkourRequest(ParkourMoveRequest& outRequest) const
{
    const Shared<Player_WorldBase> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    const Shared<TransformCom> ownerTransform = owner->Get_Transform();
    if (ownerTransform == nullptr)
        return false;

    const Vec3 start = ownerTransform->Get_WorldPosition();
    const Vec3 forward = Flatten_Normalized(ownerTransform->Get_WorldForward(), Vec3::Look);

    Vec3 targetPoint = Vec3::Zero;
    Vec3 faceDir = forward;

    const Shared<GameObject> candidateObject = owner->Get_ParkourCandidateObject();
    const Shared<ColliderCom> candidateCollider = owner->Get_ParkourCandidateCollider();

    if (candidateObject != nullptr || candidateCollider != nullptr)
    {
        if (!Is_MapStaticMeshCandidate(candidateObject))
            return false;

        const Vec3 colliderCenter = Resolve_ColliderWorldCenter(candidateCollider, candidateObject);
        faceDir = Flatten_Normalized(colliderCenter - start, forward);

        float topY = colliderCenter.y;
        if (candidateCollider != nullptr)
            topY += candidateCollider->Get_ShapeDesc().extents.y;

        const float height = topY - start.y;
        if (height < kMinParkourHeight || height > kMaxParkourHeight)
            return false;

        targetPoint = start + faceDir * (kLandingForwardOffset + 0.35f);
        targetPoint.y = topY;

        outRequest.actionType =
            (height > kLowMantleMaxHeight) ? ParkourActionType::HighMantle : ParkourActionType::LowMantle;
    }
    else
    {
        PhysXQueryHit frontHit{};
        const Vec3 chestOrigin = start + Vec3(0.f, 1.05f, 0.f);
        if (!GAME->Raycast_Environment(chestOrigin, forward, kForwardProbeDistance, frontHit))
            return false;

        const Vec3 topProbeOrigin = frontHit.position + forward * kLandingForwardOffset + Vec3(0.f, kMaxParkourHeight + 0.35f, 0.f);

        PhysXQueryHit topHit{};
        if (!GAME->Raycast_Environment(topProbeOrigin, Vec3(0.f, -1.f, 0.f), kMaxParkourHeight + 0.8f, topHit))
            return false;

        const float height = topHit.position.y - start.y;
        if (height < kMinParkourHeight || height > kMaxParkourHeight)
            return false;

        targetPoint = topHit.position + forward * kLandingForwardOffset;
        faceDir = forward;

        outRequest.actionType =
            (height > kLowMantleMaxHeight) ? ParkourActionType::HighMantle : ParkourActionType::LowMantle;
    }

    outRequest.startPoint = start;
    outRequest.targetPoint = targetPoint;
    outRequest.faceDir = faceDir;
    return true;
}

bool Player_ParkourAction::Is_MapStaticMeshCandidate(const Shared<GameObject>& object) const
{
    if (object == nullptr)
        return false;

    if (object->Get_ObjectType() != ObjectType::StaticMesh)
        return false;

    return dynamic_pointer_cast<MapStaticMesh>(object) != nullptr;
}

Vec3 Player_ParkourAction::Resolve_ColliderWorldCenter(const Shared<ColliderCom>& collider, const Shared<GameObject>& object) const
{
    if (object != nullptr)
    {
        if (const Shared<TransformCom> objectTransform = object->Get_Transform())
        {
            const Vec3 localCenter = collider != nullptr ? collider->Get_ShapeDesc().center : Vec3::Zero;
            return Vec3::Transform(localCenter, objectTransform->Get_WorldMatrix());
        }
    }

    if (collider != nullptr)
        return collider->Get_ShapeDesc().center;

    return Vec3::Zero;
}

Shared<Player_ParkourAction> Player_ParkourAction::Create(const Shared<Player_WorldBase>& owner)
{
    auto instance = make_shared<Player_ParkourAction>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[PLAYER PARKOUR ACTION] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

void Player_ParkourAction::Free()
{
    Player_State::Free();
}
