#include "pch.h"
#include "Player_WorldAction.h"

#include "Body_Player.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"

Player_WorldAction::Player_WorldAction(const Shared<Player_WorldBase>& owner)
    : Player_State{ owner }
{
}

HRESULT Player_WorldAction::Initialize()
{
    return Player_State::Initialize();
}

using plAnim = Body_Player::PlayerWorldAnimation;

bool Player_WorldAction::EnterInvoke()
{
    if (_stateCom->Get_CurrentPlayerState() == PlayerState::WorldAction)
        return false;

    _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Explo), 0.2f, false);

    return true;
}

void Player_WorldAction::Update(float timeDelta)
{
    PlayerControllerCom::RigidControl rigidControl{};
    rigidControl.rigidMovement = true;
    rigidControl.rigidJump = true;
    _controllerCom->Set_RigidControl(rigidControl);

    if (_bodyPlayer->Get_AnimationProgress() >= 0.9f)
    {
        if (Has_MoveInput())
            _stateCom->Change_State(PlayerState::Move);
        else
            _stateCom->Change_State(PlayerState::Idle);
    }
}

void Player_WorldAction::LateUpdate(float timeDelta)
{
}

void Player_WorldAction::ExitInvoke()
{
}

Shared<Player_WorldAction> Player_WorldAction::Create(const Shared<Player_WorldBase>& owner)
{
    auto instance = make_shared<Player_WorldAction>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[PLAYER WORLD ACTION] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

void Player_WorldAction::Free()
{
    Player_State::Free();
}
