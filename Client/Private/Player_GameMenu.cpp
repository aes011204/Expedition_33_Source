#include "pch.h"
#include "Player_GameMenu.h"

Player_GameMenu::Player_GameMenu(const Shared<Player_WorldBase>& owner)
    : Player_State{ owner }
{
}

HRESULT Player_GameMenu::Initialize()
{
    return Player_State::Initialize();
}

bool Player_GameMenu::EnterInvoke()
{
    return true;
}

void Player_GameMenu::Update(float timeDelta)
{
    
}

void Player_GameMenu::LateUpdate(float timeDelta)
{
    
}

void Player_GameMenu::ExitInvoke()
{
    
}

Shared<Player_GameMenu> Player_GameMenu::Create(const Shared<Player_WorldBase>& owner)
{
    auto instance = make_shared<Player_GameMenu>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[PLAYER GAMEMENU] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

void Player_GameMenu::Free()
{
    Player_State::Free();
}
