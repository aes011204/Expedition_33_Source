#include "pch.h"
#include "Player_RopeAction.h"

#include "Body_Player.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "MovementCom.h"
#include "ModelCom.h"
#include "PhysX_Types.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"
#include "Player_WorldBase.h"
#include "TransformCom.h"

namespace
{
    constexpr float kStartToTravelProgress = 0.8f;
    constexpr float kRopeActionSpeedScale = 1.5f;
    constexpr float kUpDownRopeTravelSpeed = 7.0f * kRopeActionSpeedScale;
    constexpr float kFinishDistance = 0.03f;
    constexpr float kAnimationBlendDuration = 0.12f;
    constexpr float kLandingProbeUp = 0.75f;
    constexpr float kLandingProbeDownDistance = 2.5f;

    bool Is_EnvironmentHit(const PhysXQueryHit& hit)
    {
        if (!hit.hit)
            return false;

        if (hit.collider != nullptr)
            return hit.collider->Get_Channel() == Collision_Channel::Environment;

        return hit.staticHandle.Is_Valid();
    }

    Body_Player::PlayerWorldAnimation Get_StartAnimation(WorldInteractionSpot::RopeDirection direction)
    {
        return direction == WorldInteractionSpot::RopeDirection::Down
            ? Body_Player::PlayerWorldAnimation::Pl_RopeDown_Start
            : Body_Player::PlayerWorldAnimation::Pl_RopeUp_Start;
    }

    Body_Player::PlayerWorldAnimation Get_EndAnimation(WorldInteractionSpot::RopeDirection direction)
    {
        return direction == WorldInteractionSpot::RopeDirection::Down
            ? Body_Player::PlayerWorldAnimation::Pl_RopeDown_End
            : Body_Player::PlayerWorldAnimation::Pl_RopeUp_End;
    }

    Body_Player::PlayerWorldAnimation Get_IdleAnimation(WorldInteractionSpot::RopeDirection direction)
    {
        return direction == WorldInteractionSpot::RopeDirection::Down
            ? Body_Player::PlayerWorldAnimation::Pl_RopeDownIdle
            : Body_Player::PlayerWorldAnimation::Pl_RopeUpIdle;
    }

    void Apply_RopeAnimationSpeed(const Shared<ModelCom>& model)
    {
        if (model == nullptr)
            return;

        model->Set_AnimationSpeedByState(ETOI(Body_Player::PlayerWorldAnimation::Pl_RopeUp_Start), kRopeActionSpeedScale);
        model->Set_AnimationSpeedByState(ETOI(Body_Player::PlayerWorldAnimation::Pl_RopeUpIdle), kRopeActionSpeedScale);
        model->Set_AnimationSpeedByState(ETOI(Body_Player::PlayerWorldAnimation::Pl_RopeUp_End), kRopeActionSpeedScale);
        model->Set_AnimationSpeedByState(ETOI(Body_Player::PlayerWorldAnimation::Pl_RopeDown_Start), kRopeActionSpeedScale);
        model->Set_AnimationSpeedByState(ETOI(Body_Player::PlayerWorldAnimation::Pl_RopeDownIdle), kRopeActionSpeedScale);
        model->Set_AnimationSpeedByState(ETOI(Body_Player::PlayerWorldAnimation::Pl_RopeDown_End), kRopeActionSpeedScale);
    }
}

Player_RopeAction::Player_RopeAction(const Shared<Player_WorldBase>& owner)
    : Player_State(owner)
{
}

HRESULT Player_RopeAction::Initialize()
{
    return Player_State::Initialize();
}

bool Player_RopeAction::EnterInvoke()
{
    Refresh_BodyPlayer();

    const Shared<Player_WorldBase> owner = _owner.lock();
    if (owner == nullptr || !owner->Is_WorldInteracting() ||
        _bodyPlayer == nullptr || _controllerCom == nullptr || _stateCom == nullptr || _movementCom == nullptr)
        return false;

    _request = owner->Get_WorldInteractionRequest();
    if (!Is_UpDownRopeRequest())
    {
        owner->End_WorldInteraction();
        return false;
    }

    const Shared<TransformCom> transform = owner->Get_Transform();
    if (transform == nullptr)
        return false;

    Apply_RopeAnimationSpeed(_bodyPlayer->Get_Model());

    _phase = RopePhase::Start;
    _startAlignFromPos = transform->Get_WorldPosition();
    _travelStartPos = _request.ropeDirection == WorldInteractionSpot::RopeDirection::Down
        ? _request.ropePoint
        : Vec3(_request.ropePoint.x, _startAlignFromPos.y, _request.ropePoint.z);
    _travelEndPos = _request.targetRopePoint;
    _endMoveStartPos = _travelEndPos;
    _endMoveTargetPos = Resolve_LandingPoint(_request.targetPoint);
    _travelDistance = 0.f;
    _totalTravelDistance = (_travelEndPos - _travelStartPos).Length();
    _prevRootMotionRatio = 0.f;
    _endLookStartRootMotionRatio = 0.f;
    _hasActiveRequest = true;
    _startLookFromRotation = transform->Get_WorldRotationQuaternion();
    _startLookToRotation = Resolve_TargetLookRotation(transform, _travelStartPos);

    _movementCom->Sync_RuntimePositionToTransform();

    if (_bodyPlayer != nullptr)
    {
        _bodyPlayer->Set_AnimationSpeed(1.f);
        _bodyPlayer->Set_AnimationByState(
            ETOI(Get_StartAnimation(_request.ropeDirection)),
            kAnimationBlendDuration,
            false);
    }

    return true;
}

void Player_RopeAction::Update(float timeDelta)
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

    switch (_phase)
    {
    case RopePhase::Start:
        Update_StartPhase(timeDelta);
        break;
    case RopePhase::Travel:
        Update_TravelPhase(timeDelta);
        break;
    case RopePhase::End:
        Update_EndPhase(timeDelta);
        break;
    case RopePhase::None:
    default:
        Finish_RopeAction();
        if (_stateCom != nullptr)
            _stateCom->Change_State(PlayerState::Idle);
        break;
    }
}

void Player_RopeAction::LateUpdate(float timeDelta)
{
}

void Player_RopeAction::ExitInvoke()
{
    if (const Shared<Player_WorldBase> owner = _owner.lock())
        owner->End_WorldInteraction();

    if (_controllerCom != nullptr)
        _controllerCom->Set_RigidControl(PlayerControllerCom::RigidControl{});

    _phase = RopePhase::None;
    _hasActiveRequest = false;
    _travelDistance = 0.f;
    _totalTravelDistance = 0.f;
    _prevRootMotionRatio = 0.f;
}

void Player_RopeAction::Enter_TravelPhase()
{
    _phase = RopePhase::Travel;
    _travelDistance = 0.f;

    if (const Shared<Player_WorldBase> owner = _owner.lock())
    {
        if (const Shared<TransformCom> transform = owner->Get_Transform())
        {
            transform->Set_WorldPosition(_travelStartPos);
            Apply_LookBlend(transform, _startLookFromRotation, _startLookToRotation, 1.f);
            _movementCom->Sync_RuntimePositionToTransform();
        }
    }

    if (_bodyPlayer != nullptr)
    {
        _bodyPlayer->Set_AnimationSpeed(1.f);
        _bodyPlayer->Set_AnimationByState(
            ETOI(Get_IdleAnimation(_request.ropeDirection)),
            kAnimationBlendDuration,
            true);
    }

    if (_totalTravelDistance <= kFinishDistance)
        Enter_EndPhase();
}

void Player_RopeAction::Enter_EndPhase()
{
    _phase = RopePhase::End;
    _prevRootMotionRatio = 0.f;

    if (const Shared<Player_WorldBase> owner = _owner.lock())
    {
        if (const Shared<TransformCom> transform = owner->Get_Transform())
        {
            _endMoveStartPos = transform->Get_WorldPosition();
            _endMoveTargetPos = Resolve_LandingPoint(_request.targetPoint);
            _endLookFromRotation = transform->Get_WorldRotationQuaternion();
            _endLookToRotation = Resolve_TargetLookRotation(transform, _endMoveTargetPos);
            _movementCom->Sync_RuntimePositionToTransform();
        }
    }

    if (_bodyPlayer != nullptr)
    {
        _bodyPlayer->Set_AnimationSpeed(1.f);
        if (const Shared<ModelCom> model = _bodyPlayer->Get_Model())
        {
            model->Set_AnimationByState(
                ETOI(Get_EndAnimation(_request.ropeDirection)),
                kAnimationBlendDuration,
                false);
            _prevRootMotionRatio = model->Get_RootMotionTranslationRatio();
            _endLookStartRootMotionRatio = _prevRootMotionRatio;
        }
        else
        {
            _bodyPlayer->Set_AnimationByState(
                ETOI(Get_EndAnimation(_request.ropeDirection)),
                kAnimationBlendDuration,
                false);
            _endLookStartRootMotionRatio = 0.f;
        }
    }
}

void Player_RopeAction::Update_StartPhase(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    const float progress = _bodyPlayer != nullptr ? _bodyPlayer->Get_AnimationProgress() : 1.f;
    const float alignRatio = kStartToTravelProgress <= 0.0001f
        ? 1.f
        : std::clamp(progress / kStartToTravelProgress, 0.f, 1.f);

    if (const Shared<Player_WorldBase> owner = _owner.lock())
    {
        if (const Shared<TransformCom> transform = owner->Get_Transform())
        {
            const Vec3 nextPos = Vec3::Lerp(_startAlignFromPos, _travelStartPos, alignRatio);
            transform->Set_WorldPosition(nextPos);
            Apply_LookBlend(transform, _startLookFromRotation, _startLookToRotation, alignRatio);
            _movementCom->Sync_RuntimePositionToTransform();
        }
    }

    if (progress >= kStartToTravelProgress || (_bodyPlayer != nullptr && _bodyPlayer->Is_AnimationFinished()))
        Enter_TravelPhase();
}

void Player_RopeAction::Update_TravelPhase(float timeDelta)
{
    const Shared<Player_WorldBase> owner = _owner.lock();
    if (owner == nullptr)
        return;

    const Shared<TransformCom> transform = owner->Get_Transform();
    if (transform == nullptr)
        return;

    const Vec3 currentPos = transform->Get_WorldPosition();
    const Vec3 toTarget = _travelEndPos - currentPos;
    const float remain = toTarget.Length();

    if (remain <= kFinishDistance)
    {
        transform->Set_WorldPosition(_travelEndPos);
        _movementCom->Sync_RuntimePositionToTransform();
        Enter_EndPhase();
        return;
    }

    const float moveDistance = std::max(0.f, timeDelta) * kUpDownRopeTravelSpeed;
    const Vec3 moveDir = toTarget / remain;
    const Vec3 nextPos = moveDistance >= remain
        ? _travelEndPos
        : currentPos + moveDir * moveDistance;

    transform->Set_WorldPosition(nextPos);
    _movementCom->Sync_RuntimePositionToTransform();

    if ((nextPos - _travelEndPos).Length() <= kFinishDistance)
        Enter_EndPhase();
}

void Player_RopeAction::Update_EndPhase(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    const Shared<Player_WorldBase> owner = _owner.lock();
    if (owner == nullptr)
        return;

    const Shared<TransformCom> transform = owner->Get_Transform();
    const Shared<ModelCom> model = _bodyPlayer != nullptr ? _bodyPlayer->Get_Model() : nullptr;
    if (transform == nullptr || model == nullptr)
    {
        Finish_RopeAction();
        return;
    }

    const float currentRatio = model->Get_RootMotionTranslationRatio();
    const float lookRatio = std::clamp(
        (currentRatio - _endLookStartRootMotionRatio) / std::max(0.0001f, 1.f - _endLookStartRootMotionRatio),
        0.f,
        1.f);
    float moveRatio = currentRatio - _prevRootMotionRatio;
    if (moveRatio < 0.f)
        moveRatio = 0.f;

    _prevRootMotionRatio = currentRatio;

    const Vec3 currentPos = transform->Get_WorldPosition();
    Vec3 toTarget = _endMoveTargetPos - currentPos;
    const float remain = toTarget.Length();

    if (remain > 0.01f && moveRatio > 0.f)
    {
        const Vec3 path = _endMoveTargetPos - _endMoveStartPos;
        Vec3 moveDelta = path * moveRatio;

        if (moveDelta.Length() > remain)
            moveDelta = toTarget;

        transform->Set_WorldPosition(currentPos + moveDelta);
        Apply_LookBlend(transform, _endLookFromRotation, _endLookToRotation, lookRatio);
        _movementCom->Sync_RuntimePositionToTransform();
    }
    else
    {
        Apply_LookBlend(transform, _endLookFromRotation, _endLookToRotation, lookRatio);
        _movementCom->Sync_RuntimePositionToTransform();
    }

    if (_bodyPlayer->Is_AnimationFinished())
    {
        transform->Set_WorldPosition(_endMoveTargetPos);
        Apply_LookBlend(transform, _endLookFromRotation, _endLookToRotation, 1.f);
        _movementCom->Force_Land();
        _movementCom->Sync_RuntimePositionToTransform();
        Finish_RopeAction();

        if (Has_MoveInput())
            _stateCom->Change_State(PlayerState::Move);
        else
            _stateCom->Change_State(PlayerState::Idle);
    }
}

bool Player_RopeAction::Is_UpDownRopeRequest() const
{
    if (_request.type != WorldInteractionSpot::WorldInteractionType::RopeTouch)
        return false;

    return _request.ropeDirection == WorldInteractionSpot::RopeDirection::Up ||
        _request.ropeDirection == WorldInteractionSpot::RopeDirection::Down;
}

Vec3 Player_RopeAction::Resolve_LandingPoint(const Vec3& targetPoint) const
{
    PhysXQueryHit groundHit{};
    const Vec3 probeOrigin = targetPoint + Vec3(0.f, kLandingProbeUp, 0.f);
    if (GAME->Raycast_Environment(probeOrigin, Vec3(0.f, -1.f, 0.f), kLandingProbeDownDistance, groundHit) &&
        Is_EnvironmentHit(groundHit))
    {
        return groundHit.position;
    }

    return targetPoint;
}

Quat Player_RopeAction::Resolve_TargetLookRotation(const Shared<TransformCom>& transform, const Vec3& targetPoint) const
{
    if (transform == nullptr)
        return Quat::Identity;

    const Quat currentRotation = transform->Get_WorldRotationQuaternion();
    Vec3 direction = targetPoint - transform->Get_WorldPosition();
    direction.y = 0.f;

    if (direction.LengthSquared() <= 0.0001f)
        return currentRotation;

    direction.Normalize();
    Quat targetRotation = Quat::CreateFromYawPitchRoll(atan2f(direction.x, direction.z), 0.f, 0.f);
    targetRotation.Normalize();
    return targetRotation;
}

void Player_RopeAction::Apply_LookBlend(
    const Shared<TransformCom>& transform,
    const Quat& fromRotation,
    const Quat& toRotation,
    float ratio) const
{
    if (transform == nullptr)
        return;

    Quat blendedRotation = Quat::Slerp(fromRotation, toRotation, std::clamp(ratio, 0.f, 1.f));
    if (blendedRotation.LengthSquared() <= 0.0001f)
        return;

    blendedRotation.Normalize();
    transform->Set_WorldRotationQuaternion(blendedRotation);
}

void Player_RopeAction::Finish_RopeAction()
{
    if (const Shared<Player_WorldBase> owner = _owner.lock())
        owner->End_WorldInteraction();

    _phase = RopePhase::None;
    _hasActiveRequest = false;
}

Shared<Player_RopeAction> Player_RopeAction::Create(const Shared<Player_WorldBase>& owner)
{
    auto instance = make_shared<Player_RopeAction>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[PLAYER ROPE ACTION] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

void Player_RopeAction::Free()
{
    Player_State::Free();
}
