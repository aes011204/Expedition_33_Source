#include "pch.h"
#include "Player_Idle.h"

#include "Body_Player.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"

Player_Idle::Player_Idle(const Shared<Player_WorldBase>& owner)
    : Player_State{ owner }
{
}

using plAnim = Body_Player::PlayerWorldAnimation;

HRESULT Player_Idle::Initialize()
{
    _exitAnim.emplace(ETOI(plAnim::Pl_EndSprint));
    _exitAnim.emplace(ETOI(plAnim::Pl_EndRun));
    _exitAnim.emplace(ETOI(plAnim::Pl_EndWalk));

    return Player_State::Initialize();
}

bool Player_Idle::EnterInvoke()
{
    auto stateIndex = _stateCom->Get_CurrentPlayerState();
    uint32 animIndex = _bodyPlayer->Get_AnimationStateIndex();

    const float enterBlendTime = // Free Aim일 때에는 빠르게
        (stateIndex == PlayerState::FreeAim) ? _freeAimExitBlendTime : _idleBlendTime;
        
    bool hasFailedStateAnimation = true;
    switch (stateIndex)
    {
    case PlayerState::Move:
        _bodyPlayer->Set_AnimationSpeed(1.5f);
        if (animIndex == ETOI(plAnim::Pl_Sprint))
            hasFailedStateAnimation = _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_EndSprint), 0.25f, false);
        else if (animIndex == ETOI(plAnim::Pl_Run))
            hasFailedStateAnimation = _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_EndRun), 0.25f, false);
        else if (animIndex == ETOI(plAnim::Pl_Walk))
            hasFailedStateAnimation = _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_EndWalk), 0.25f, false);
        break;
    default:
        _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Idle_0), enterBlendTime, true);
        break;
    }

    if (!hasFailedStateAnimation)
        _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Idle_0), enterBlendTime, true);

    Reset_IdleTimer();
    
    if (_isFirstState)
    {
        _isFirstState = false;
    }

    return true;
}

void Player_Idle::Update(float timeDelta)
{
    uint32 animIndex = _bodyPlayer->Get_AnimationStateIndex();

    //if (IsFalling())
    //{
    //    _stateCom->Change_State(PlayerState::Jump);
    //    return;
    //}

    if (IsEnterFreeAim())
    {
        _stateCom->Change_State(PlayerState::FreeAim);
        return;
    }

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

    if (_exitAnim.contains(animIndex) && _bodyPlayer->Get_AnimationProgress() >= 0.76f)
    {
        _bodyPlayer->Set_AnimationSpeed(1.f);
        _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Idle_0), 0.3f, true);
        return;
    }

    if (_isRoutineIdle)
    {
        bool isBlending = _bodyPlayer->Is_AnimationBlending();

        if (!isBlending)
        {
            if (_bodyPlayer->Get_AnimationProgress() >= 0.86f)
            {
                _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Idle_0), _idleBlendTime, true);
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

    uint32 index = Math::RandNum(ETOI(plAnim::Pl_Idle_1), ETOI(plAnim::Pl_Idle_6));
    if (_bodyPlayer->Set_AnimationByState(index, _idleBlendTime, false))
    {
        _isRoutineIdle = true;
    }
    else
    {
        if (state != ETOI(plAnim::Pl_Idle_0))
            _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Idle_0), _idleBlendTime, true);

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
