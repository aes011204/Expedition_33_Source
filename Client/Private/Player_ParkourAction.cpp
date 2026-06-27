#include "pch.h"
#include "Player_ParkourAction.h"

#include "Body_Player.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "MovementCom.h"
#include "PhysX_Types.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"
#include "Player_WorldBase.h"
#include "TransformCom.h"

namespace
{
    constexpr float kRootMotionScale = 0.01f;
    constexpr float kMinParkourHeight = 0.20f;
    constexpr float kMaxParkourHeight = 1.75f;
    constexpr float kLandingForwardOffset = 0.20f;
    constexpr float kTargetBackOffset = 0.08f;
    constexpr float kTopProbeExtraUp = 0.35f;
    constexpr float kWalkableNormalY = 0.65f;
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

    const char* Get_ParkourActionName(Player_ParkourAction::ParkourActionType actionType)
    {
        return actionType == Player_ParkourAction::ParkourActionType::HighMantle ? "High" : "Low";
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
    Refresh_BodyPlayer();
    if (_bodyPlayer == nullptr || _controllerCom == nullptr || _stateCom == nullptr || _movementCom == nullptr)
    {
        LOG_WARN("[Parkour] Reject Enter: missing required component");
        return false;
    }

    if (!_hasPreparedRequest)
    {
        LOG_WARN("[Parkour] Reject Enter: missing prepared request");
        return false;
    }

    _hasPreparedRequest = false;
    _warpStartPos = _request.startPoint;
    _hasWarp = true;

    if (auto owner = _owner.lock())
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
    }

    return true;
}

void Player_ParkourAction::Update(float timeDelta)
{
    if (_controllerCom == nullptr || _stateCom == nullptr || _bodyPlayer == nullptr || _movementCom == nullptr)
    {
        if (_stateCom != nullptr)
            _stateCom->Change_State(PlayerState::Idle);
        return;
    }

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
        Vec3 pathDir = toTarget;
        pathDir.Normalize();
        const float progress = _bodyPlayer->Get_AnimationProgress();
        const float t = SmoothStep01(progress / kFallbackWarpEndProgress);
        Vec3 moveDelta = Vec3::Lerp(_warpStartPos, _request.targetPoint, t) - currentPos;

        if (Shared<ModelCom> model = _bodyPlayer->Get_Model())
        {
            const RootMotionFrame& rm = model->Get_RootTransformVelocity();
            Vec3 localDelta = rm.position * kRootMotionScale;

            if (localDelta.LengthSquared() > 0.000001f)
            {
                const float moveDistance = localDelta.Length();
                const Vec3 rootMoveDelta = pathDir * moveDistance;

                if (rootMoveDelta.LengthSquared() > moveDelta.LengthSquared())
                    moveDelta = rootMoveDelta;
            }
        }

        if (moveDelta.Length() > remain)
            moveDelta = pathDir * remain;

        transform->Set_WorldPosition(currentPos + moveDelta);
        transform->LookAt(transform->Get_WorldPosition() + _request.faceDir);

        _movementCom->Sync_RuntimePositionToTransform();
    }

    if (_bodyPlayer->Is_AnimationFinished())
    {
        transform->Set_WorldPosition(_request.targetPoint);
        _movementCom->Sync_RuntimePositionToTransform();

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
    if (_controllerCom != nullptr)
        _controllerCom->Set_RigidControl(PlayerControllerCom::RigidControl{});

    _hasWarp = false;
    _hasPreparedRequest = false;
}

bool Player_ParkourAction::Build_ParkourRequest(ParkourActionType actionType, ParkourMoveRequest& outRequest) const
{
    const char* actionName = Get_ParkourActionName(actionType);
    auto owner = _owner.lock();
    if (owner == nullptr)
    {
        LOG_WARN("[Parkour] Reject {}: missing owner", actionName);
        return false;
    }

    auto ownerTransform = owner->Get_Transform();
    if (ownerTransform == nullptr)
    {
        LOG_WARN("[Parkour] Reject {}: missing owner transform", actionName);
        return false;
    }

    const Vec3 start = ownerTransform->Get_WorldPosition();
    const Matrix worldMatrix = ownerTransform->Get_WorldMatrix();
    const Vec3 forward = Flatten_Normalized(ownerTransform->Get_WorldForward(), Vec3::Look);
    const bool highAction = actionType == ParkourActionType::HighMantle;

    PhysXQueryHit frontHit{};
    const Vec3 frontOrigin = highAction
        ? Vec3::Transform(Get_ParkourHighProbeCenter(), worldMatrix)
        : Vec3::Transform(Get_ParkourLowProbeCenter(), worldMatrix);
    const float frontDistance = highAction ? Get_ParkourHighProbeDistance() : Get_ParkourLowProbeDistance();

    const bool hasFrontHit =
        GAME->Raycast_Environment(frontOrigin, forward, frontDistance, frontHit) &&
        Is_EnvironmentHit(frontHit);

    if (!hasFrontHit)
    {
        if (!highAction)
        {
            LOG_WARN("[Parkour] Reject {}: front ray miss. origin=({:.2f},{:.2f},{:.2f}) dir=({:.2f},{:.2f},{:.2f}) distance={:.2f}",
                actionName,
                frontOrigin.x, frontOrigin.y, frontOrigin.z,
                forward.x, forward.y, forward.z,
                frontDistance);
            return false;
        }

        LOG_WARN("[Parkour] High front ray miss. Try top fallback. origin=({:.2f},{:.2f},{:.2f}) dir=({:.2f},{:.2f},{:.2f}) distance={:.2f}",
            frontOrigin.x, frontOrigin.y, frontOrigin.z,
            forward.x, forward.y, forward.z,
            frontDistance);
    }

    Vec3 topProbeOrigin = hasFrontHit
        ? frontHit.position + forward * kLandingForwardOffset
        : frontOrigin + forward * frontDistance;
    topProbeOrigin.y = start.y + kMaxParkourHeight + kTopProbeExtraUp;

    PhysXQueryHit topHit{};
    const float topProbeDistance = kMaxParkourHeight + kTopProbeExtraUp + 0.5f;
    if (!GAME->Raycast_Environment(topProbeOrigin, Vec3(0.f, -1.f, 0.f), topProbeDistance, topHit))
    {
        LOG_WARN("[Parkour] Reject {}: top ray miss. origin=({:.2f},{:.2f},{:.2f}) distance={:.2f}",
            actionName,
            topProbeOrigin.x, topProbeOrigin.y, topProbeOrigin.z,
            topProbeDistance);
        return false;
    }

    if (!Is_EnvironmentHit(topHit))
    {
        LOG_WARN("[Parkour] Reject {}: top hit is not environment", actionName);
        return false;
    }

    if (topHit.normal.y < kWalkableNormalY)
    {
        LOG_WARN("[Parkour] Reject {}: top normal too steep. normalY={:.2f}, min={:.2f}",
            actionName, topHit.normal.y, kWalkableNormalY);
        return false;
    }

    const float height = topHit.position.y - start.y;
    if (height < kMinParkourHeight || height > kMaxParkourHeight)
    {
        LOG_WARN("[Parkour] Reject {}: height out of range. height={:.2f}, min={:.2f}, max={:.2f}",
            actionName, height, kMinParkourHeight, kMaxParkourHeight);
        return false;
    }

    outRequest.startPoint = start;
    outRequest.targetPoint = topHit.position - forward * kTargetBackOffset;
    outRequest.faceDir = forward;
    outRequest.actionType = actionType;

    LOG_WARN("[Parkour] Accept {}: target=({:.2f},{:.2f},{:.2f}) height={:.2f}",
        actionName,
        outRequest.targetPoint.x, outRequest.targetPoint.y, outRequest.targetPoint.z,
        height);

    return true;
}

bool Player_ParkourAction::Is_EnvironmentHit(const PhysXQueryHit& hit) const
{
    if (!hit.hit)
        return false;

    if (hit.collider != nullptr)
        return hit.collider->Get_Channel() == Collision_Channel::Environment;

    return hit.staticHandle.Is_Valid();
}

bool Player_ParkourAction::Try_PrepareAction(ParkourActionType actionType)
{
    _hasPreparedRequest = false;

    ParkourMoveRequest request{};
    if (!Build_ParkourRequest(actionType, request))
    {
        return false;
    }

    _request = request;
    _hasPreparedRequest = true;
    return true;
}

void Player_ParkourAction::Clear_PreparedAction()
{
    _hasPreparedRequest = false;
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
