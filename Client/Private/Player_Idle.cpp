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

using pl = Body_Player::PlayerAnimation;

bool Player_Idle::EnterInvoke()
{
    Set_IdleAnimation();
    _isFirstState = false;

    return true;
}

void Player_Idle::Update(float timeDelta)
{
    bool isFinished = _bodyPlayer->Is_AnimationFinished();
    
    

    if (isFinished) {
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
    uint32 randIndex = static_cast<uint32>(GAME->Random(0.f, 1.f));
    if (randIndex == 0) {
        _bodyPlayer->Set_Animation(ETOI(pl::Pl_Idle_0), 0.1f, false);
    }
    else {
        _bodyPlayer->Set_Animation(ETOI(pl::Pl_Idle_1), 0.1f, false);
    }
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
