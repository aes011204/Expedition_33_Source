#include "pch.h"
#include "Player_Move.h"

#include "Body_Player.h"
#include "PlayerStateMachineCom.h"

Player_Move::Player_Move(const Shared<Player_WorldBase>& owner)
    : Player_State{ owner } {}

HRESULT Player_Move::Initialize()
{
    return Player_State::Initialize();
}

bool Player_Move::EnterInvoke()
{
    return true;
}

void Player_Move::Update(float timeDelta)
{
    uint32 animIndex = _bodyPlayer->Get_AnimationStateIndex();

    if (IsEnterIdle())
    {
        _stateCom->Change_State(PlayerState::Idle);
        return;
    }

    if (IsEnterJump())
    {
        _stateCom->Change_State(PlayerState::Jump);
        return;
    }

    if (IsEnterWorldAction())
    {
        _stateCom->Change_State(PlayerState::WorldAction);
        return;
    }

    using plAnim = Body_Player::PlayerWorldAnimation;
    if (IsSprintMove())
    {
        if (animIndex != ETOI(plAnim::Pl_Sprint))
            _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Sprint), 0.2f, true);
    }
    else if (IsWalkMove())
    {
        if (animIndex != ETOI(plAnim::Pl_Walk))
            _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Walk), 0.2f, true);
    }
    else
    {
        if (animIndex != ETOI(plAnim::Pl_Run))
        {
            _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Run), 0.2f, true);
        }
    }
}

void Player_Move::LateUpdate(float timeDelta)
{
 
}

void Player_Move::ExitInvoke()
{
    
}

Shared<Player_Move> Player_Move::Create(const Shared<Player_WorldBase>& owner)
{
    auto instance = make_shared<Player_Move>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[PLAYER MOVE] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

void Player_Move::Free()
{
    Player_State::Free();
}
