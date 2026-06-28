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
    constexpr float kUpDownRopeTravelSpeed = 3.5f;
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

    Body_Player::PlayerWorldAnimation Get_TravelAnimation(WorldInteractionSpot::RopeDirection direction)
    {
        return direction == WorldInteractionSpot::RopeDirection::Down
            ? Body_Player::PlayerWorldAnimation::Pl_Rope_Down
            : Body_Player::PlayerWorldAnimation::Pl_Rope_Up;
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

    _phase = RopePhase::Start;
    _travelStartPos = transform->Get_WorldPosition();
    _travelEndPos = _request.targetPoint;
    _endMoveStartPos = _travelEndPos;
    _endMoveTargetPos = Resolve_LandingPoint(_request.targetPoint);
    _travelDistance = 0.f;
    _totalTravelDistance = (_travelEndPos - _travelStartPos).Length();
    _prevRootMotionRatio = 0.f;
    _hasActiveRequest = true;

    const Vec3 faceDir = Resolve_FaceDirection();
    transform->LookAt(transform->Get_WorldPosition() + faceDir);
    _movementCom->Sync_RuntimePositionToTransform();

    if (_bodyPlayer != nullptr)
    {
        _bodyPlayer->Set_AnimationSpeed(1.f);
        _bodyPlayer->Set_AnimationByState(
            ETOI(Body_Player::PlayerWorldAnimation::Pl_Rope_Start),
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

    if (_bodyPlayer != nullptr)
    {
        _bodyPlayer->Set_AnimationSpeed(1.f);
        _bodyPlayer->Set_AnimationByState(
            ETOI(Get_TravelAnimation(_request.ropeDirection)),
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
            transform->LookAt(transform->Get_WorldPosition() + Resolve_FaceDirection());
            _movementCom->Sync_RuntimePositionToTransform();
        }
    }

    if (_bodyPlayer != nullptr)
    {
        _bodyPlayer->Set_AnimationSpeed(1.f);
        if (const Shared<ModelCom> model = _bodyPlayer->Get_Model())
        {
            model->Set_AnimationByState(
                ETOI(Body_Player::PlayerWorldAnimation::Pl_Rope_End),
                kAnimationBlendDuration,
                false);
            _prevRootMotionRatio = model->Get_RootMotionTranslationRatio();
        }
        else
        {
            _bodyPlayer->Set_AnimationByState(
                ETOI(Body_Player::PlayerWorldAnimation::Pl_Rope_End),
                kAnimationBlendDuration,
                false);
        }
    }
}

void Player_RopeAction::Update_StartPhase(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    const float progress = _bodyPlayer != nullptr ? _bodyPlayer->Get_AnimationProgress() : 1.f;
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

    _travelDistance += std::max(0.f, timeDelta) * kUpDownRopeTravelSpeed;
    const float t = _totalTravelDistance <= 0.0001f
        ? 1.f
        : std::clamp(_travelDistance / _totalTravelDistance, 0.f, 1.f);

    const Vec3 nextPos = Vec3::Lerp(_travelStartPos, _travelEndPos, t);
    transform->Set_WorldPosition(nextPos);
    transform->LookAt(nextPos + Resolve_FaceDirection());
    _movementCom->Sync_RuntimePositionToTransform();

    if (t >= 1.f)
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
        transform->LookAt(transform->Get_WorldPosition() + Resolve_FaceDirection());
        _movementCom->Sync_RuntimePositionToTransform();
    }

    if (_bodyPlayer->Is_AnimationFinished())
    {
        transform->Set_WorldPosition(_endMoveTargetPos);
        transform->LookAt(_endMoveTargetPos + Resolve_FaceDirection());
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

Vec3 Player_RopeAction::Resolve_FaceDirection() const
{
    Vec3 faceDir = _request.faceDir;
    faceDir.y = 0.f;

    if (faceDir.LengthSquared() <= 0.0001f)
        faceDir = Vec3::Look;

    faceDir.Normalize();
    return faceDir;
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
