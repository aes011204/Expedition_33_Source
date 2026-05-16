#include "pch.h"
#include "Player_Idle.h"

#include "Body_Player.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"

Player_Idle::Player_Idle(const Shared<Player>& owner)
    : Player_State{ owner }
{
}

HRESULT Player_Idle::Initialize()
{
    return Player_State::Initialize();
}

using pl = Body_Player::PlayerWorldAnimation;

bool Player_Idle::EnterInvoke()
{
    Set_IdleAnimation();
    _isFirstState = false;

    return true;
}

void Player_Idle::Update(float timeDelta)
{
    bool isFinished = _bodyPlayer->Is_AnimationFinished();
    auto& input = Get_MovementInput();

    if (IsEnterWorldAction())
    {
        _stateCom->Change_State(PlayerState::WorldAction);
        return;
    }

    if (IsEnterJump())
    {
        _stateCom->Change_State(PlayerState::Jump);
        return;
    }

    if (IsEnterMove())
    {
        _stateCom->Change_State(PlayerState::Move);
        return;
    }

    if (isFinished)
    {
        Set_IdleAnimation();
    }
}

void Player_Idle::LateUpdate(float timeDelta)
{
    
}

void Player_Idle::ExitInvoke()
{

}

void Player_Idle::Set_IdleAnimation() const
{
    if (_isFirstState)
    {
        _bodyPlayer->Set_AnimationByState(ETOI(pl::Pl_Idle_0), 0.1f, true);
        return;
    }

    auto state = _stateCom->Get_CurrentPlayerState();
    if (state == PlayerState::WorldAction)
        _bodyPlayer->Set_AnimationByState(ETOI(pl::Pl_Idle_0), 0.25f, true);
    else
        _bodyPlayer->Set_AnimationByState(ETOI(pl::Pl_Idle_0), 0.2f, true);
}

Shared<Player_Idle> Player_Idle::Create(const Shared<Player>& owner)
{
    auto instance = make_shared<Player_Idle>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[PLAYER IDLE] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

void Player_Idle::Free()
{
    Player_State::Free();
}
