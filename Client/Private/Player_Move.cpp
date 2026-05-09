#include "pch.h"
#include "Player_Move.h"

Player_Move::Player_Move(const Shared<Player>& owner)
    : Player_State{ owner } {}

HRESULT Player_Move::Initialize()
{
    return Player_State::Initialize();
}

bool Player_Move::EnterInvoke()
{
    return Player_State::EnterInvoke();
}

void Player_Move::Update(float timeDelta)
{
 
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
