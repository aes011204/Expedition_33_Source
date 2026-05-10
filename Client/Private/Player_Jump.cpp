#include "pch.h"
#include "Player_Jump.h"

Player_Jump::Player_Jump(const Shared<Player>& owner)
    : Player_State{ owner } {}

HRESULT Player_Jump::Initialize()
{
    return Player_State::Initialize();
}

bool Player_Jump::EnterInvoke()
{
    return Player_State::EnterInvoke();
}

void Player_Jump::Update(float timeDelta)
{
    
}

void Player_Jump::LateUpdate(float timeDelta)
{
    
}

void Player_Jump::ExitInvoke()
{
    
}

Shared<Player_Jump> Player_Jump::Create(const Shared<Player>& owner)
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
