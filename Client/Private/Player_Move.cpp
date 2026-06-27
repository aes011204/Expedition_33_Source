#include "pch.h"
#include "Player_Move.h"

#include "Body_Player.h"
#include "PlayerStateMachineCom.h"
#include "Player_WorldBase.h"

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

    const auto prevState = _stateCom->Get_CurrentPlayerState();
    _enterMoveBlendTime = _defaultMoveBlendTime;
    _useOneShotEnterBlendTime = false;

    if (prevState == PlayerState::FreeAim)
    {
        _enterMoveBlendTime = _freeAimExitMoveBlendTime;
        _useOneShotEnterBlendTime = true;
    }

    return true;
}

void Player_Move::Update(float timeDelta)
{
    uint32 animIndex = _bodyPlayer->Get_AnimationStateIndex();

    if (IsEnterFreeAim())
    {
        _stateCom->Change_State(PlayerState::FreeAim);
        return;
    }

    if (IsEnterIdle())
    {
        _stateCom->Change_State(PlayerState::Idle);
        return;
    }

    WorldInteractionSpot::WorldInteractionRequest interactionRequest{};
    Shared<GameObject> interactionSpot{};
    if (Try_BuildWorldInteractionRequest(interactionRequest, interactionSpot))
    {
        if (const Shared<Player_WorldBase> owner = _owner.lock())
        {
            if (owner->Begin_WorldInteraction(interactionSpot, interactionRequest))
            {
                bool changed = false;
                switch (interactionRequest.type)
                {
                case WorldInteractionSpot::WorldInteractionType::WallClimb:
                    changed = _stateCom->Change_State(PlayerState::Climb);
                    break;
                case WorldInteractionSpot::WorldInteractionType::RopeTouch:
                case WorldInteractionSpot::WorldInteractionType::RopeLook:
                default:
                    changed = _stateCom->Change_State(PlayerState::RopeAction);
                    break;
                }

                if (!changed)
                    owner->End_WorldInteraction();
                else
                    return;
            }
        }
    }

    const float locomotionBlendTime = _useOneShotEnterBlendTime ? _enterMoveBlendTime : _defaultMoveBlendTime;

    if (IsSprintMove())
    {
        if (isInvokePreSprint)
        {
            if (_bodyPlayer->Is_AnimationFinished())
            {
                _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Sprint), locomotionBlendTime, true);
                isInvokePreSprint = false;
                _useOneShotEnterBlendTime = false;
            }
        }
        else if (animIndex != ETOI(plAnim::Pl_Sprint))
        {
            if (_bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_PreSprint), locomotionBlendTime, false))
            {
                isInvokePreSprint = true;
                _useOneShotEnterBlendTime = false;
            }
            else
            {
                _bodyPlayer->Set_AnimationSpeed(1.5f);
                _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Sprint), locomotionBlendTime, true);
                _useOneShotEnterBlendTime = false;
            }
        }
    }
    else if (IsWalkMove())
    {
        isInvokePreSprint = false;
        _bodyPlayer->Set_AnimationSpeed(1.f);
        if (animIndex != ETOI(plAnim::Pl_Walk))
        {
            _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Walk), locomotionBlendTime, true);
            _useOneShotEnterBlendTime = false; // 첫 전환에만 적용
        }
    }
    else
    {
        isInvokePreSprint = false;
        _bodyPlayer->Set_AnimationSpeed(1.35f);
        if (animIndex != ETOI(plAnim::Pl_Run))
        {
            _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Run), locomotionBlendTime, true);
            _useOneShotEnterBlendTime = false; // 첫 전환에만 적용
        }
    }
}

void Player_Move::LateUpdate(float timeDelta)
{
    Draw_ParkourDebugRays();
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
