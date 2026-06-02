#include "pch.h"
#include "Player_Jump.h"

#include "Body_Player.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"
#include "Player_WorldBase.h"

Player_Jump::Player_Jump(const Shared<Player_WorldBase>& owner)
	: Player_State{ owner } {}

HRESULT Player_Jump::Initialize()
{
	return Player_State::Initialize();
}

using plAnim = Body_Player::PlayerWorldAnimation;

bool Player_Jump::EnterInvoke()
{
	_fallTime = 0.0f;

	uint32 animIndex = _bodyPlayer->Get_AnimationStateIndex();

    if (IsEnterJump())
    {
        if (animIndex == ETOI(plAnim::Pl_Run) || animIndex == ETOI(plAnim::Pl_Sprint))
            _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_JumpRun), 0.2f, false);
        else
            _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_JumpWalk), 0.2f, false);
    }
    else
    {
        _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Fall), 0.2f, true);
    }

	return true;
}

void Player_Jump::Update(float timeDelta)
{
	uint32 animIndex = _bodyPlayer->Get_AnimationStateIndex();
	float curProgress = _bodyPlayer->Get_AnimationProgress();
	bool isFinished = _bodyPlayer->Is_AnimationFinished();
	bool isGrounded = _controllerCom->Is_Grounded();
	bool isBlending = _bodyPlayer->Is_AnimationBlending();

	if (!isGrounded)
	{
		_fallTime += timeDelta;

		if (isFinished) 
		{
			if (animIndex == ETOI(plAnim::Pl_JumpWalk) || animIndex == ETOI(plAnim::Pl_JumpRun))
			{
				_bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Fall), 0.5f, true);
			}
		}
	}
	else
	{
		if (animIndex != ETOI(plAnim::Pl_LandLight) && animIndex != ETOI(plAnim::Pl_LandHeavy))
		{
			if (_fallTime >= 1.f)
			{
				_bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_LandHeavy), 0.2f, false);
                _bodyPlayer->Set_AnimationSpeed(1.5f);
			}
			else
			{
				_bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_LandLight), 0.2f, false);
                _bodyPlayer->Set_AnimationSpeed(1.5f);
			}
			return;
		}

		if (animIndex == ETOI(plAnim::Pl_LandLight))
		{
			if (Has_MoveInput())
			{
				_stateCom->Change_State(PlayerState::Move);
			}
			else if (curProgress >= 0.6f)
			{
				_stateCom->Change_State(PlayerState::Idle);
			}
		}
		else if (animIndex == ETOI(plAnim::Pl_LandHeavy))
		{
			if (curProgress >= 0.75f)
			{
				if (Has_MoveInput())
				{
					_stateCom->Change_State(PlayerState::Move);
				}
				else
				{
					_stateCom->Change_State(PlayerState::Idle);
				}
			}
		}
	}
}

void Player_Jump::LateUpdate(float timeDelta)
{
	uint32 animIndex = _bodyPlayer->Get_AnimationStateIndex();
	bool isFinished = _bodyPlayer->Is_AnimationFinished();

	PlayerControllerCom::RigidControl rigidControl{};
	rigidControl.rigidMovement = false;
	rigidControl.rigidJump = true;

	if (animIndex == ETOI(plAnim::Pl_LandHeavy) && !isFinished)
	{
		rigidControl.rigidMovement = true;
        Vec3 rootVelocity = _bodyPlayer->Get_RootMotionPosition();
        Vec3 currentVel = Get_Velocity();

        rootVelocity.y = currentVel.y;

        rigidControl.rigidLook = true;
        rigidControl.rigidVelocity = rootVelocity;
	}

	_controllerCom->Set_RigidControl(rigidControl);
}

void Player_Jump::ExitInvoke()
{
    _bodyPlayer->Set_AnimationSpeed(1.f);
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
