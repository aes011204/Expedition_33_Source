#include "pch.h"
#include "Player_State.h"

#include "InputCom.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"
#include "Player_WorldBase.h"
#include "Body_Player.h"
#include "TransformCom.h"

namespace
{
    Vec3 Normalize_ParkourLook(Vec3 look)
    {
        look.y = 0.f;
        if (look.LengthSquared() <= 0.0001f)
            return Vec3::Look;

        look.Normalize();
        return look;
    }
}

Player_State::Player_State(const Shared<Player_WorldBase>& owner)
    : State{}
{
    _owner = owner;
}

HRESULT Player_State::Initialize()
{
    if (_owner.expired()) return E_FAIL;

    return State::Initialize();
}

void Player_State::Begin_Play()
{
    if (_owner.expired()) return;

    _controllerCom = _owner.lock()->Get_Component<PlayerControllerCom>().get();
    if (nullptr == _controllerCom)
        MSG_BOX("[PLAYER STATE] : Failed to Begin Player_WorldBase State");

    _stateCom = _owner.lock()->Get_Component<PlayerStateMachineCom>().get();
    if (nullptr == _stateCom)
        MSG_BOX("[PLAYER STATE] : Failed to Begin Player_WorldBase State");

    _movementCom = _owner.lock()->Get_Component<MovementCom>().get();
    if (nullptr == _movementCom)
        MSG_BOX("[PLAYER STATE] : Failed to Begin Player_WorldBase State");

    Refresh_BodyPlayer();
}

void Player_State::Refresh_BodyPlayer()
{
    if (_owner.expired())
    {
        _bodyPlayer = nullptr;
        return;
    }

    _bodyPlayer = dynamic_cast<Body_Player*>(_owner.lock()->Get_PartObject(L"Part_Body"));
}

bool Player_State::EnterInvoke()
{
    return State::EnterInvoke();
}

void Player_State::Update(float timeDelta)
{
    
}

void Player_State::LateUpdate(float timeDelta)
{
    
}

void Player_State::ExitInvoke()
{
    State::ExitInvoke();
}

bool Player_State::Has_MoveInput() const
{
    return _controllerCom->Has_MoveInput();
}

bool Player_State::Is_WalkToggled() const
{
    return _controllerCom->Is_WalkToggled();
}

const MovementCom::MovementInput& Player_State::Get_MovementInput() const
{
    return _controllerCom->Get_MovementInput();
}

Vec3 Player_State::Get_Velocity() const
{
    return _controllerCom->Get_Velocity();
}

bool Player_State::Has_LocomotionIntent() const
{
    return _controllerCom->Has_LocomotionIntent();
}

bool Player_State::IsSprintMove() const
{
     return _controllerCom->Should_SprintMove();
}

bool Player_State::IsWalkMove() const
{
    return _controllerCom->Should_WalkMove();
}

bool Player_State::Is_MovementBlocked() const
{
    return _controllerCom->Is_MovementBlocked();
}

bool Player_State::IsFalling() const
{
    return !_controllerCom->Is_Grounded();
}

bool Player_State::IsFreeAimShoot() const
{
    return _controllerCom->Is_RightMouseClicked() && _controllerCom->Allows_ActionInput();
}

float Player_State::Get_ParkourLowProbeDistance() const
{
    const Shared<Player_WorldBase> owner = _owner.lock();
    return owner != nullptr ? owner->Get_ParkourLowProbeDistance() : 0.15f;
}

float Player_State::Get_ParkourHighProbeDistance() const
{
    const Shared<Player_WorldBase> owner = _owner.lock();
    return owner != nullptr ? owner->Get_ParkourHighProbeDistance() : 0.15f;
}

Vec3 Player_State::Get_ParkourLowProbeCenter() const
{
    const Shared<Player_WorldBase> owner = _owner.lock();
    return owner != nullptr ? owner->Get_ParkourLowProbeCenter() : Vec3(0.f, 1.00f, 0.850f);
}

Vec3 Player_State::Get_ParkourHighProbeCenter() const
{
    const Shared<Player_WorldBase> owner = _owner.lock();
    return owner != nullptr ? owner->Get_ParkourHighProbeCenter() : Vec3(0.f, 1.75f, 0.850f);
}

void Player_State::Draw_ParkourDebugRays() const
{
    const Shared<Player_WorldBase> owner = _owner.lock();
    if (owner == nullptr)
        return;

    const Shared<TransformCom> transform = owner->Get_Transform();
    if (transform == nullptr)
        return;

    const Matrix worldMatrix = transform->Get_WorldMatrix();
    const Vec3 lowStart = Vec3::Transform(Get_ParkourLowProbeCenter(), worldMatrix);
    const Vec3 highStart = Vec3::Transform(Get_ParkourHighProbeCenter(), worldMatrix);
    const Vec3 look = Normalize_ParkourLook(transform->Get_WorldForward());

    DebugLineDesc lowDesc{};
    lowDesc.start = lowStart;
    lowDesc.end = lowStart + look * Get_ParkourLowProbeDistance();
    lowDesc.style.color = Color(0.45f, 0.75f, 1.f, 1.f);
    lowDesc.style.depthEnabled = false;
    GAME->Draw_DebugLine(lowDesc);

    DebugLineDesc highDesc{};
    highDesc.start = highStart;
    highDesc.end = highStart + look * Get_ParkourHighProbeDistance();
    highDesc.style.color = Color(1.f, 0.55f, 1.f, 1.f);
    highDesc.style.depthEnabled = false;
    GAME->Draw_DebugLine(highDesc);
}

bool Player_State::Try_BuildWorldInteractionRequest(WorldInteractionSpot::WorldInteractionRequest& outRequest, Shared<GameObject>& outSpot) const
{
    outSpot = nullptr;

    if (_controllerCom == nullptr || !_controllerCom->Allows_ActionInput())
        return false;

    const auto& input = Get_MovementInput();
    if (!input.isInteractionDown)
        return false;

    const Shared<Player_WorldBase> owner = _owner.lock();
    if (owner == nullptr || owner->Is_WorldInteracting())
        return false;

    const Shared<GameObject> candidate = owner->Get_WorldInteractionCandidate();
    if (candidate == nullptr)
        return false;

    const auto* provider = dynamic_cast<IWorldInteractionProvider*>(candidate.get());
    if (provider == nullptr || !provider->Build_InteractionRequest(owner, outRequest))
        return false;

    outSpot = candidate;
    return true;
}

bool Player_State::IsEnterWorldAction() const
{
    const auto& input = Get_MovementInput();

    return input.isSuddenAttack && _controllerCom->Allows_ActionInput();
}

bool Player_State::IsEnterFreeAim() const
{
    const auto& input = Get_MovementInput();

    return input.isMouseRPress && !input.isMouseRUp && _controllerCom->Allows_ActionInput();
}

bool Player_State::IsEnterJump() const
{
    const auto& input = Get_MovementInput();

    return input.isJumpDown && _controllerCom->Allows_ActionInput();
}

bool Player_State::IsEnterMove() const
{
    return Has_LocomotionIntent() && !Is_MovementBlocked();
}

bool Player_State::IsEnterIdle() const
{
    return !Has_LocomotionIntent() || Is_MovementBlocked();
}

void Player_State::Free()
{
    _owner.reset();
    _bodyPlayer = nullptr;

    State::Free();
}
