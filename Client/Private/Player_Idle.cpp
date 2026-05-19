#include "pch.h"
#include "Player_Idle.h"

#include "Body_Player.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"

Player_Idle::Player_Idle(const Shared<Player_WorldBase>& owner)
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
    _bodyPlayer->Set_AnimationByState(ETOI(pl::Pl_Idle_0), _idleBlendTime, true);
    Reset_IdleTimer();
    
    if (_isFirstState)
    {
        _isFirstState = false;
    }

    return true;
}

void Player_Idle::Update(float timeDelta)
{
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


    if (_isRoutineIdle)
    {
        bool isBlending = _bodyPlayer->Is_AnimationBlending();

        if (!isBlending)
        {
            if (_bodyPlayer->Get_AnimationProgress() >= 0.86f)
            {
                _bodyPlayer->Set_AnimationByState(ETOI(pl::Pl_Idle_0), _idleBlendTime, true);
                Reset_IdleTimer();
            }
        }
    }
}

void Player_Idle::LateUpdate(float timeDelta)
{
    if (!_isRoutineIdle)
    {
        _idleElapsedTime += timeDelta;

        if (_idleElapsedTime >= _idleThreshHoldTime)
        {
            Set_IdleAnimation();
        }
    }
}

void Player_Idle::ExitInvoke()
{

}

void Player_Idle::Reset_IdleTimer()
{
    _idleThreshHoldTime = 3.f + Math::RandNum(0.f, 2.f);
    _idleElapsedTime = 0.f;
    _isRoutineIdle = false;
}

void Player_Idle::Set_IdleAnimation()
{
    uint32 state = _bodyPlayer->Get_AnimationStateIndex();

    uint32 index = Math::RandNum(ETOI(pl::Pl_Idle_1), ETOI(pl::Pl_Idle_6));
    if (_bodyPlayer->Set_AnimationByState(index, _idleBlendTime, false))
    {
        _isRoutineIdle = true;
    }
    else
    {
        if (state != ETOI(pl::Pl_Idle_0))
            _bodyPlayer->Set_AnimationByState(ETOI(pl::Pl_Idle_0), _idleBlendTime, true);

        Reset_IdleTimer();
    }
}

Shared<Player_Idle> Player_Idle::Create(const Shared<Player_WorldBase>& owner)
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
