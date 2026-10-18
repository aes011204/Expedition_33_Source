#include "pch.h"
#include "Player_Jump.h"
#include "ClientInstance.h"
#include "Body_Player.h"
#include "GameInstance.h"
#include "Player_ParkourAction.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"
#include "Player_WorldBase.h"
#include "TargetCamera.h"

Player_Jump::Player_Jump(const Shared<Player_WorldBase>& owner)
	: Player_State{ owner } {}

namespace
{
    constexpr float kHighParkourProbeInterval = 0.f;
	constexpr float kLandHeavyFallTimeThreshold = 1.f;
    constexpr float kLandHeavyRootMotionScale = 0.5f;

    bool Try_EnterParkourAction(
        PlayerStateMachineCom* stateCom,
        Player_ParkourAction::ParkourActionType actionType)
    {
        if (stateCom == nullptr)
        {
            return false;
        }

        const Shared<Player_ParkourAction> parkourAction =
            dynamic_pointer_cast<Player_ParkourAction>(stateCom->Get_State(PlayerState::Parkour));
        if (parkourAction == nullptr)
        {
            return false;
        }

        if (!parkourAction->Try_PrepareAction(actionType))
        {
            return false;
        }

        const bool changed = stateCom->Change_State(PlayerState::Parkour);
        if (!changed)
            parkourAction->Clear_PreparedAction();

        return changed;
    }

    bool Try_EnterHighParkourAction(PlayerStateMachineCom* stateCom)
    {
        return Try_EnterParkourAction(stateCom, Player_ParkourAction::ParkourActionType::HighMantle);
    }

    void Snap_TargetCameraVerticalToOwner(const Shared<Player_WorldBase>& owner)
    {
        if (owner == nullptr || !CLIENT->Is_ActiveHero(static_pointer_cast<GameObject>(owner)))
            return;

        if (const Shared<TargetCamera> targetCamera =
            dynamic_pointer_cast<TargetCamera>(GAME->Get_CameraByType(ObjectType::TargetCamera)))
        {
            targetCamera->Snap_TargetVerticalPosition(owner->Get_Transform());
        }
    }
}

HRESULT Player_Jump::Initialize()
{
	return Player_State::Initialize();
}

using plAnim = Body_Player::PlayerWorldAnimation;

bool Player_Jump::EnterInvoke()
{
	_fallTime = _initialFallTime;
    _initialFallTime = 0.f;
    _parkourProbeElapsed = 0.f;
    _hasLeftGroundForParkourProbe = false;

    const Shared<Player_WorldBase> owner = _owner.lock();
    const bool isActiveHero = owner != nullptr &&
    CLIENT->Is_ActiveHero(static_pointer_cast<GameObject>(owner));

    _canProbeParkour = isActiveHero &&
        IsEnterJump() &&
        owner != nullptr &&
        !owner->Is_ParkourBlockedByCurrentLevel();
    _lowParkourProbeDone = false;
    _repeatHighParkourProbe = false;

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
        Snap_TargetCameraVerticalToOwner(owner);
    }

	return true;
}

void Player_Jump::Update(float timeDelta)
{
	uint32 animIndex = _bodyPlayer->Get_AnimationStateIndex();
	float curProgress = _bodyPlayer->Get_AnimationProgress();
	bool isFinished = _bodyPlayer->Is_AnimationFinished();
	bool isGrounded = _controllerCom->Is_Grounded();
    const bool isLandingAnimation =
        animIndex == ETOI(plAnim::Pl_LandLight) ||
        animIndex == ETOI(plAnim::Pl_LandHeavy);

    if (!isGrounded)
        _hasLeftGroundForParkourProbe = true;

    const Shared<Player_WorldBase> owner = _owner.lock();
    const bool isActiveHero = owner != nullptr &&

    CLIENT->Is_ActiveHero(static_pointer_cast<GameObject>(owner));

    if (isActiveHero && _canProbeParkour && !isLandingAnimation)
    {
        if (!_lowParkourProbeDone)
        {
            _lowParkourProbeDone = true;
            _parkourProbeElapsed = 0.f;

            if (Try_EnterParkourAction(this->_stateCom, Player_ParkourAction::ParkourActionType::LowMantle))
                return;

            _repeatHighParkourProbe = true;
            if (Try_EnterHighParkourAction(this->_stateCom))
                return;
        }
        else if (_repeatHighParkourProbe)
        {
            if (isGrounded && _hasLeftGroundForParkourProbe)
            {
                _repeatHighParkourProbe = false;
            }
            else
            {
                _parkourProbeElapsed += timeDelta;

                if (_parkourProbeElapsed >= kHighParkourProbeInterval)
                {
                    _parkourProbeElapsed = 0.f;

                    if (Try_EnterHighParkourAction(this->_stateCom))
                        return;
                }
            }
        }
    }

    if (animIndex == ETOI(plAnim::Pl_Fall))
        Snap_TargetCameraVerticalToOwner(owner);

	if (!isGrounded)
	{
		_fallTime += timeDelta;

		if (isFinished) 
		{
			if (animIndex == ETOI(plAnim::Pl_JumpWalk) || animIndex == ETOI(plAnim::Pl_JumpRun))
			{
				_bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Fall), 0.5f, true);
                Snap_TargetCameraVerticalToOwner(owner);
			}
		}
	}
	else
	{
        if (!_hasLeftGroundForParkourProbe)
            return;

		if (animIndex != ETOI(plAnim::Pl_LandLight) && animIndex != ETOI(plAnim::Pl_LandHeavy))
		{
			if (_fallTime >= kLandHeavyFallTimeThreshold)
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
    //Draw_ParkourDebugRays();

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

		rootVelocity.x *= kLandHeavyRootMotionScale;
		rootVelocity.z *= kLandHeavyRootMotionScale;
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
