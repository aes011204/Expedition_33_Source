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

using plAnim = Body_Player::PlayerWorldAnimation;

bool Player_Move::EnterInvoke()
{
    isInvokePreSprint = false;

    return true;
}

void Player_Move::Update(float timeDelta)
{
    uint32 animIndex = _bodyPlayer->Get_AnimationStateIndex();

    //if (IsFalling())
    //{
    //    _stateCom->Change_State(PlayerState::Jump);
    //    return;
    //}

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

    if (IsSprintMove())
    {
        if (isInvokePreSprint)
        {
            if (_bodyPlayer->Is_AnimationFinished())
            {
                _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Sprint), 0.35f, true);
                isInvokePreSprint = false;
            }
        }
        else if (animIndex != ETOI(plAnim::Pl_Sprint))
        {
            if (_bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_PreSprint), 0.35f, false))
            {
                isInvokePreSprint = true;
            }
            else
            {
                _bodyPlayer->Set_AnimationSpeed(1.5f);
                _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Sprint), 0.35f, true);
            }
        }
    }
    else if (IsWalkMove())
    {
        isInvokePreSprint = false;
        _bodyPlayer->Set_AnimationSpeed(1.f);
        if (animIndex != ETOI(plAnim::Pl_Walk))
            _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Walk), 0.35f, true);
    }
    else
    {
        isInvokePreSprint = false;
        _bodyPlayer->Set_AnimationSpeed(1.35f);
        if (animIndex != ETOI(plAnim::Pl_Run))
        {
            _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Run), 0.35f, true);
        }
    }
}

void Player_Move::LateUpdate(float timeDelta)
{
 
}

void Player_Move::ExitInvoke()
{
    isInvokePreSprint = false;
    _bodyPlayer->Set_AnimationSpeed(1.f);
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
