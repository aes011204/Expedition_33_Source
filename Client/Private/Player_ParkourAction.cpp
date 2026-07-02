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
#include "TargetCamera.h"
#include "TransformCom.h"

namespace
{
    constexpr float kParkourGroundYTolerance = 0.05f;
    constexpr float kParkourTopProbeForwardOffset = 0.20f;
    constexpr float kParkourTopProbeUp = 0.35f;
    constexpr float kParkourTopProbeDownDistance = 0.95f;
    constexpr float kParkourProbeDistance = 0.75f;
    constexpr float kParkourLowProbeHeight = 1.00f;
    constexpr float kLowParkourMaxTargetHeight = 1.15f;
    constexpr float kParkourHighProbeHeight = 2.00f;
    constexpr float kParkourFinishDistance = 0.03f;
    constexpr float kParkourAnimationBlendDuration = 0.1f;
    constexpr float kLowParkourAnimationStartFrame = 0.f;
    constexpr float kHighParkourAnimationStartFrame = 0.f;

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
        switch (actionType)
        {
        case Player_ParkourAction::ParkourActionType::HighMantle:
            return "High";
        case Player_ParkourAction::ParkourActionType::LowMantle:
        default:
            return "Low";
        }
    }

    Body_Player::PlayerWorldAnimation Get_ParkourAnimation(Player_ParkourAction::ParkourActionType actionType)
    {
        return actionType == Player_ParkourAction::ParkourActionType::LowMantle
            ? Body_Player::PlayerWorldAnimation::Pl_Parkour_Low
            : Body_Player::PlayerWorldAnimation::PL_Parkour_High;
    }

    float Get_ParkourAnimationStartFrame(Player_ParkourAction::ParkourActionType actionType)
    {
        switch (actionType)
        {
        case Player_ParkourAction::ParkourActionType::HighMantle:
            return kHighParkourAnimationStartFrame;
        case Player_ParkourAction::ParkourActionType::LowMantle:
        default:
            return kLowParkourAnimationStartFrame;
        }
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
        //LOG_WARN("[Parkour] Reject Enter: missing required component");
        return false;
    }

    if (!_hasPreparedRequest)
    {
        //LOG_WARN("[Parkour] Reject Enter: missing prepared request");
        return false;
    }

    _hasPreparedRequest = false;
    _moveStartPos = _request.startPoint;
    _prevRootMotionRatio = 0.f;
    _hasActiveRequest = true;

    //LOG_WARN("[Parkour] Enter {}: start=({:.2f},{:.2f},{:.2f}) target=({:.2f},{:.2f},{:.2f})",
    //    Get_ParkourActionName(_request.actionType),
    //    _request.startPoint.x, _request.startPoint.y, _request.startPoint.z,
    //    _request.targetPoint.x, _request.targetPoint.y, _request.targetPoint.z);

    if (auto owner = _owner.lock())
    {
        if (const Shared<TransformCom> transform = owner->Get_Transform())
        {
            transform->Set_WorldPosition(_request.startPoint);
            transform->LookAt(_request.startPoint + _request.faceDir);
            _movementCom->Sync_RuntimePositionToTransform();
        }
    }

    if (_bodyPlayer != nullptr)
    {
        const Body_Player::PlayerWorldAnimation anim = Get_ParkourAnimation(_request.actionType);
        const float animationStartFrame = Get_ParkourAnimationStartFrame(_request.actionType);

        _bodyPlayer->Set_AnimationSpeed(1.f);
        if (Shared<ModelCom> model = _bodyPlayer->Get_Model())
        {
            model->Set_AnimationByState(ETOI(anim), kParkourAnimationBlendDuration, false, animationStartFrame);
            _prevRootMotionRatio = model->Get_RootMotionTranslationRatio();
        }
        else
        {
            _bodyPlayer->Set_AnimationByState(ETOI(anim), kParkourAnimationBlendDuration, false);
        }
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
    if (owner == nullptr || !_hasActiveRequest)
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

    const Shared<ModelCom> model = _bodyPlayer->Get_Model();
    if (model == nullptr)
    {
        _stateCom->Change_State(PlayerState::Idle);
        return;
    }

    const Vec3 currentPos = transform->Get_WorldPosition();
    Vec3 toTarget = _request.targetPoint - currentPos;
    float remain = toTarget.Length();

    const float currentRatio = model->Get_RootMotionTranslationRatio();
    float moveRatio = currentRatio - _prevRootMotionRatio;
    if (moveRatio < 0.f)
        moveRatio = 0.f;

    _prevRootMotionRatio = currentRatio;

    if (remain > 0.01f && moveRatio > 0.f)
    {
        const Vec3 path = _request.targetPoint - _moveStartPos;
        Vec3 moveDelta = path * moveRatio;

        if (moveDelta.Length() > remain)
            moveDelta = toTarget;

        transform->Set_WorldPosition(currentPos + moveDelta);
        transform->LookAt(transform->Get_WorldPosition() + _request.faceDir);

        _movementCom->Sync_RuntimePositionToTransform();

        toTarget = _request.targetPoint - transform->Get_WorldPosition();
        remain = toTarget.Length();
    }

    if (_bodyPlayer->Is_AnimationFinished())
    {
        if (remain > kParkourFinishDistance)
        {
            //LOG_WARN("[Parkour] Finish with remaining distance. remain={:.2f}, target=({:.2f},{:.2f},{:.2f})",
            //    remain,
            //    _request.targetPoint.x, _request.targetPoint.y, _request.targetPoint.z);
        }

        _movementCom->Force_Land();
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
    if (const Shared<Player_WorldBase> owner = _owner.lock())
    {
        if (const Shared<TransformCom> transform = owner->Get_Transform())
        {
            if (const Shared<TargetCamera> targetCamera =
                dynamic_pointer_cast<TargetCamera>(GAME->Get_CameraByType(ObjectType::TargetCamera)))
            {
                targetCamera->End_ParkourVerticalLag(transform);
            }
        }
    }

    if (_controllerCom != nullptr)
        _controllerCom->Set_RigidControl(PlayerControllerCom::RigidControl{});

    _hasActiveRequest = false;
    _hasPreparedRequest = false;
    _prevRootMotionRatio = 0.f;
}

bool Player_ParkourAction::Build_ParkourRequest(ParkourActionType actionType, ParkourMoveRequest& outRequest) const
{
    if (actionType == ParkourActionType::LowMantle)
        return Build_LowParkourRequest(outRequest);

    return Build_HighParkourRequest(outRequest);
}

bool Player_ParkourAction::Build_LowParkourRequest(ParkourMoveRequest& outRequest) const
{
    return Build_ParkourRequestAtHeight(ParkourActionType::LowMantle, kParkourLowProbeHeight, outRequest);
}

bool Player_ParkourAction::Build_HighParkourRequest(ParkourMoveRequest& outRequest) const
{
    return Build_ParkourRequestAtHeight(ParkourActionType::HighMantle, kParkourHighProbeHeight, outRequest);
}

bool Player_ParkourAction::Build_ParkourRequestAtHeight(
    ParkourActionType actionType,
    float probeHeight,
    ParkourMoveRequest& outRequest) const
{
    const char* actionName = Get_ParkourActionName(actionType);
    auto owner = _owner.lock();
    if (owner == nullptr)
    {
        //LOG_WARN("[Parkour] Reject {}: missing owner", actionName);
        return false;
    }

    auto ownerTransform = owner->Get_Transform();
    if (ownerTransform == nullptr)
    {
        //LOG_WARN("[Parkour] Reject {}: missing owner transform", actionName);
        return false;
    }

    const Vec3 start = ownerTransform->Get_WorldPosition();
    const Matrix worldMatrix = ownerTransform->Get_WorldMatrix();
    const Vec3 forward = Flatten_Normalized(ownerTransform->Get_WorldForward(), Vec3::Look);
    const Vec3 frontOrigin = Vec3::Transform(Vec3(0.f, probeHeight, 0.f), worldMatrix);

    if (Build_ParkourRequestFromProbe(
        actionType,
        actionName,
        start,
        forward,
        frontOrigin,
        kParkourProbeDistance,
        outRequest))
    {
        return true;
    }

    //LOG_WARN("[Parkour] Reject {}: probe failed. localHeight={:.2f}, origin=({:.2f},{:.2f},{:.2f}), dir=({:.2f},{:.2f},{:.2f}) distance={:.2f}",
    //    actionName,
    //    probeHeight,
    //    frontOrigin.x, frontOrigin.y, frontOrigin.z,
    //    forward.x, forward.y, forward.z,
    //    kParkourProbeDistance);

    return false;
}

bool Player_ParkourAction::Build_ParkourRequestFromProbe(
    ParkourActionType actionType,
    const char* actionName,
    const Vec3& start,
    const Vec3& forward,
    const Vec3& frontOrigin,
    float frontDistance,
    ParkourMoveRequest& outRequest) const
{
    PhysXQueryHit frontHit{};
    if (!GAME->Raycast_Environment(frontOrigin, forward, frontDistance, frontHit) ||
        !Is_EnvironmentHit(frontHit))
    {
        //LOG_WARN("[Parkour] Reject {}: front ray miss. origin=({:.2f},{:.2f},{:.2f}) dir=({:.2f},{:.2f},{:.2f}) distance={:.2f}",
        //    actionName,
        //    frontOrigin.x, frontOrigin.y, frontOrigin.z,
        //    forward.x, forward.y, forward.z,
        //    frontDistance);
        return false;
    }

    Vec3 topProbeOrigin = frontHit.position + forward * kParkourTopProbeForwardOffset;
    topProbeOrigin.y = frontOrigin.y + kParkourTopProbeUp;

    PhysXQueryHit topHit{};
    if (!GAME->Raycast_Environment(topProbeOrigin, Vec3(0.f, -1.f, 0.f), kParkourTopProbeDownDistance, topHit) ||
        !Is_EnvironmentHit(topHit))
    {
        //LOG_WARN("[Parkour] Reject {}: top ray miss. origin=({:.2f},{:.2f},{:.2f}) distance={:.2f}",
        //    actionName,
        //    topProbeOrigin.x, topProbeOrigin.y, topProbeOrigin.z,
        //    kParkourTopProbeDownDistance);
        return false;
    }

    const float yDiff = fabsf(topHit.position.y - frontOrigin.y);
    const float targetHeight = topHit.position.y - start.y;
    if (actionType == ParkourActionType::LowMantle && targetHeight > kLowParkourMaxTargetHeight)
    {
        //LOG_WARN("[Parkour] Reject {}: target too high for Low. height={:.2f}, max={:.2f}, target=({:.2f},{:.2f},{:.2f})",
        //    actionName,
        //    targetHeight,
        //    kLowParkourMaxTargetHeight,
        //    topHit.position.x,
        //    topHit.position.y,
        //    topHit.position.z);
        return false;
    }

    outRequest.startPoint = start;
    outRequest.targetPoint = topHit.position;
    outRequest.faceDir = forward;
    outRequest.actionType = actionType;

    //LOG_WARN("[Parkour] Accept {}: start=({:.2f},{:.2f},{:.2f}) target=({:.2f},{:.2f},{:.2f}) height={:.2f} rayY={:.2f} diff={:.2f} tolerance={:.2f}",
    //    actionName,
    //    outRequest.startPoint.x, outRequest.startPoint.y, outRequest.startPoint.z,
    //    outRequest.targetPoint.x, outRequest.targetPoint.y, outRequest.targetPoint.z,
    //    targetHeight,
    //    frontOrigin.y,
    //    yDiff,
    //    kParkourGroundYTolerance);

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
