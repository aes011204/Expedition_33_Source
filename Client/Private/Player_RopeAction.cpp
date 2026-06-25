#include "pch.h"
#include "Player_RopeAction.h"

#include "Body_Player.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"
#include "Player_WorldBase.h"
#include "TransformCom.h"

namespace
{
    float SmoothStep01(float value)
    {
        const float t = std::clamp(value, 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
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
    const Shared<Player_WorldBase> owner = _owner.lock();
    if (owner == nullptr || !owner->Is_WorldInteracting())
        return false;

    _request = owner->Get_WorldInteractionRequest();
    _warpStartPos = owner->Get_Transform() ? owner->Get_Transform()->Get_WorldPosition() : Vec3::Zero;
    _hasWarp = true;

    if (_bodyPlayer != nullptr)
        _bodyPlayer->Set_AnimationByState(ETOI(Body_Player::PlayerWorldAnimation::Pl_Explo), 0.15f, false);

    return true;
}

void Player_RopeAction::Update(float timeDelta)
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

    const float progress = _bodyPlayer ? _bodyPlayer->Get_AnimationProgress() : 1.f;
    const float warpEnd = std::clamp(_request.warpEndProgress, 0.01f, 1.f);
    const float t = SmoothStep01(progress / warpEnd);

    if (const Shared<TransformCom> transform = owner->Get_Transform())
    {
        const Vec3 nextPos = Vec3::Lerp(_warpStartPos, _request.targetPoint, t);
        transform->Set_WorldPosition(nextPos);

        if (_request.faceDir.LengthSquared() > 0.0001f)
            transform->LookAt(nextPos + _request.faceDir);
    }

    if (progress >= 0.95f || (_bodyPlayer != nullptr && _bodyPlayer->Is_AnimationFinished()))
    {
        owner->End_WorldInteraction();

        if (Has_MoveInput())
            _stateCom->Change_State(PlayerState::Move);
        else
            _stateCom->Change_State(PlayerState::Idle);
    }
}

void Player_RopeAction::LateUpdate(float timeDelta)
{
}

void Player_RopeAction::ExitInvoke()
{
    if (const Shared<Player_WorldBase> owner = _owner.lock())
        owner->End_WorldInteraction();

    _hasWarp = false;
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
