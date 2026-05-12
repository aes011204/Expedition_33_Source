#include "pch.h"
#include "Player_Move.h"

#include "Body_Player.h"
#include "PlayerStateMachineCom.h"

Player_Move::Player_Move(const Shared<Player>& owner)
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
    auto& input = Get_MovementInput();
    uint32 animIndex = _bodyPlayer->Get_AnimationIndex();

    if (!Has_MoveInput())
    {
        _stateCom->Change_State(PlayerState::Idle);
        return;
    }

    if (input.isJumpDown)
    {
        _stateCom->Change_State(PlayerState::Jump);
        return;
    }

    using plAnim = Body_Player::PlayerAnimation;
    if (input.isRunPressed)
    {
        if (animIndex != ETOI(plAnim::Pl_Sprint))
            _bodyPlayer->Set_Animation(ETOI(plAnim::Pl_Sprint), 0.1f, true);
    }
    else if (Is_WalkToggled())
    {
        if (animIndex != ETOI(plAnim::Pl_Walk))
            _bodyPlayer->Set_Animation(ETOI(plAnim::Pl_Walk), 0.1f, true);
    }
    else
    {
        if (animIndex != ETOI(plAnim::Pl_Run))
        {
            LOG_INFO("{}", ETOI(plAnim::Pl_Run));
            _bodyPlayer->Set_Animation(ETOI(plAnim::Pl_Run), 0.1f, true);
        }
    }
}

void Player_Move::LateUpdate(float timeDelta)
{
 
}

void Player_Move::ExitInvoke()
{
    
}

Shared<Player_Move> Player_Move::Create(const Shared<Player>& owner)
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
