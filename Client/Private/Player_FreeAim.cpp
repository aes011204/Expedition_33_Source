#include "pch.h"
#include "Player_FreeAim.h"

Player_FreeAim::Player_FreeAim(const Shared<Player_WorldBase>& owner)
    : Player_State{ owner } {}
HRESULT Player_FreeAim::Initialize()
{
    return Player_State::Initialize();
}

bool Player_FreeAim::EnterInvoke()
{
    return true;
}

void Player_FreeAim::Update(float timeDelta)
{

}

void Player_FreeAim::LateUpdate(float timeDelta)
{

}

void Player_FreeAim::ExitInvoke()
{

}

Shared<Player_FreeAim> Player_FreeAim::Create(const Shared<Player_WorldBase>& owner)
{
    auto instance = make_shared<Player_FreeAim>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[PLAYER FREE AIM] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

void Player_FreeAim::Free()
{
    Player_State::Free();
}
