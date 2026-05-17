#include "pch.h"
#include "Player_Jump.h"

#include "Body_Player.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"

Player_Jump::Player_Jump(const Shared<Player_WorldBase>& owner)
    : Player_State{ owner } {}

HRESULT Player_Jump::Initialize()
{
    return Player_State::Initialize();
}

using plAnim = Body_Player::PlayerWorldAnimation;

bool Player_Jump::EnterInvoke()
{
    uint32 animIndex = _bodyPlayer->Get_AnimationStateIndex();

    if (animIndex == ETOI(plAnim::Pl_Run) || animIndex == ETOI(plAnim::Pl_Sprint))
    {
        _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_JumpRun), 0.2f, false);
    }
    else
    {
        _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_JumpWalk), 0.2f, false);
    }

    return true;
}

void Player_Jump::Update(float timeDelta)
{
    uint32 animIndex = _bodyPlayer->Get_AnimationStateIndex();
    float curProgress = _bodyPlayer->Get_AnimationProgress();
    bool isFinished = _bodyPlayer->Is_AnimationFinished();
    bool isGrounded = _controllerCom->Is_Grounded();

    if (!isGrounded)
    {
        if (isFinished)
        {
            if (animIndex == ETOI(plAnim::Pl_JumpWalk) || animIndex == ETOI(plAnim::Pl_JumpRun))
            {
                _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Fall), 0.2f, true);
            }
        }
    }
    else
    {
        if (animIndex != ETOI(plAnim::Pl_Land))
        {
            _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Land), 0.2f, false);
            return;
        }

        if (animIndex == ETOI(plAnim::Pl_Land))
        {
            if (Has_MoveInput())
                _stateCom->Change_State(PlayerState::Move);
            else if (!Has_MoveInput() && curProgress >= 0.6f)
                _stateCom->Change_State(PlayerState::Idle);
        }
    }
}

void Player_Jump::LateUpdate(float timeDelta)
{
    PlayerControllerCom::RigidControl rigidControl{};
    rigidControl.rigidMovement = false;
    rigidControl.rigidJump = true;
    _controllerCom->Set_RigidControl(rigidControl);
}

void Player_Jump::ExitInvoke()
{
    
}

Shared<Player_Jump> Player_Jump::Create(const Shared<Player_WorldBase>& owner)
{
    auto instance = make_shared<Player_Jump>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[PLAYER JUMP] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

void Player_Jump::Free()
{
    Player_State::Free();
}
