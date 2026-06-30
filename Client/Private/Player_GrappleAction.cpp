#include "pch.h"
#include "Player_GrappleAction.h"

#include "Body_Player.h"
#include "MovementCom.h"
#include "ModelCom.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"
#include "Player_WorldBase.h"
#include "TransformCom.h"

namespace
{
    constexpr float kGrappleActionSpeedScale = 1.5f;
    constexpr float kAnimationBlendDuration = 0.12f;
    constexpr float kLookPrepareDuration = 0.2f;

    float EaseInOutQuint(float ratio)
    {
        ratio = std::clamp(ratio, 0.f, 1.f);

        if (ratio < 0.5f)
        {
            const float scaled = ratio * 2.f;
            return 0.5f * scaled * scaled * scaled * scaled * scaled;
        }

        const float inverse = (1.f - ratio) * 2.f;
        return 1.f - 0.5f * inverse * inverse * inverse * inverse * inverse * inverse * inverse;
    }

    void Apply_GrappleAnimationSpeed(const Shared<ModelCom>& model)
    {
        if (model == nullptr)
            return;

        model->Set_AnimationSpeedByState(ETOI(Body_Player::PlayerWorldAnimation::Pl_Grappling), kGrappleActionSpeedScale);
    }
}

Player_GrappleAction::Player_GrappleAction(const Shared<Player_WorldBase>& owner)
    : Player_State(owner)
{
}

HRESULT Player_GrappleAction::Initialize()
{
    return Player_State::Initialize();
}

bool Player_GrappleAction::EnterInvoke()
{
    Refresh_BodyPlayer();

    const Shared<Player_WorldBase> owner = _owner.lock();
    if (owner == nullptr || !owner->Is_WorldInteracting() ||
        _bodyPlayer == nullptr || _controllerCom == nullptr || _stateCom == nullptr || _movementCom == nullptr)
        return false;

    _request = owner->Get_WorldInteractionRequest();
    if (!Is_GrapplingRequest())
    {
        owner->End_WorldInteraction();
        return false;
    }

    const Shared<TransformCom> transform = owner->Get_Transform();
    if (transform == nullptr)
        return false;

    _launchTargetPos = _request.targetPoint;
    _launchLookFromRotation = transform->Get_WorldRotationQuaternion();
    _launchLookToRotation = Resolve_TargetLookRotation(transform, _launchTargetPos);
    _lookPrepareElapsed = 0.f;
    _hasActiveRequest = true;
    _isPreparingLook = true;
    _isLaunching = false;

    return true;
}

void Player_GrappleAction::Update(float timeDelta)
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
        Finish_GrappleAction();
        return;
    }

    if (_isPreparingLook)
    {
        _lookPrepareElapsed += std::max(timeDelta, 0.f);
        const float lookRatio = kLookPrepareDuration > 0.f ? _lookPrepareElapsed / kLookPrepareDuration : 1.f;

        Apply_LookBlend(transform, _launchLookFromRotation, _launchLookToRotation, lookRatio);
        _movementCom->Sync_RuntimePositionToTransform();

        if (lookRatio >= 1.f)
            Begin_GrappleLaunch(transform);

        return;
    }

    if (!_isLaunching)
    {
        Finish_GrappleAction();
        _stateCom->Change_State(PlayerState::Idle);
        return;
    }

    const float progress = _bodyPlayer->Get_AnimationProgress();
    const float moveRatio = EaseInOutQuint(progress);
    const Vec3 nextPos = Vec3::Lerp(_launchStartPos, _launchTargetPos, moveRatio);

    transform->Set_WorldPosition(nextPos);
    Apply_LookBlend(transform, _launchLookToRotation, _launchLookToRotation, 1.f);
    _movementCom->Sync_RuntimePositionToTransform();

    if (_bodyPlayer->Is_AnimationFinished())
    {
        transform->Set_WorldPosition(_launchTargetPos);
        Apply_LookBlend(transform, _launchLookFromRotation, _launchLookToRotation, 1.f);
        _movementCom->Force_Land();
        _movementCom->Sync_RuntimePositionToTransform();
        Finish_GrappleAction();

        if (Has_MoveInput())
            _stateCom->Change_State(PlayerState::Move);
        else
            _stateCom->Change_State(PlayerState::Idle);
    }
}

void Player_GrappleAction::LateUpdate(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void Player_GrappleAction::ExitInvoke()
{
    if (const Shared<Player_WorldBase> owner = _owner.lock())
        owner->End_WorldInteraction();

    if (_controllerCom != nullptr)
        _controllerCom->Set_RigidControl(PlayerControllerCom::RigidControl{});

    _hasActiveRequest = false;
    _isPreparingLook = false;
    _isLaunching = false;
}

bool Player_GrappleAction::Is_GrapplingRequest() const
{
    return _request.type == WorldInteractionSpot::WorldInteractionType::RopeLook &&
        _request.ropeDirection == WorldInteractionSpot::RopeDirection::Launch;
}

Quat Player_GrappleAction::Resolve_TargetLookRotation(const Shared<TransformCom>& transform, const Vec3& targetPoint) const
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

void Player_GrappleAction::Apply_LookBlend(
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

void Player_GrappleAction::Begin_GrappleLaunch(const Shared<TransformCom>& transform)
{
    if (transform == nullptr || _bodyPlayer == nullptr || _movementCom == nullptr)
        return;

    _isPreparingLook = false;
    _isLaunching = true;
    _launchStartPos = transform->Get_WorldPosition();

    Apply_LookBlend(transform, _launchLookToRotation, _launchLookToRotation, 1.f);
    Apply_GrappleAnimationSpeed(_bodyPlayer->Get_Model());
    _movementCom->Sync_RuntimePositionToTransform();

    _bodyPlayer->Set_AnimationSpeed(1.f);
    _bodyPlayer->Set_AnimationByState(
        ETOI(Body_Player::PlayerWorldAnimation::Pl_Grappling),
        kAnimationBlendDuration,
        false);
}

void Player_GrappleAction::Finish_GrappleAction()
{
    if (const Shared<Player_WorldBase> owner = _owner.lock())
        owner->End_WorldInteraction();

    _hasActiveRequest = false;
    _isPreparingLook = false;
    _isLaunching = false;
}

Shared<Player_GrappleAction> Player_GrappleAction::Create(const Shared<Player_WorldBase>& owner)
{
    auto instance = make_shared<Player_GrappleAction>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[PLAYER GRAPPLE ACTION] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

void Player_GrappleAction::Free()
{
    Player_State::Free();
}
