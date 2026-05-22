#include "pch.h"
#include "Player_GameMenu.h"

#include "Body_Player.h"

Player_GameMenu::Player_GameMenu(const Shared<Player_WorldBase>& owner)
    : Player_State{ owner }
{
}

HRESULT Player_GameMenu::Initialize()
{
    return Player_State::Initialize();
}

using plAnim = Body_Player::PlayerWorldAnimation;

bool Player_GameMenu::EnterInvoke()
{
    _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_GameMenu_Intro), 0.2f, false);
    _isPlayingIntro = true;
    _pendingAnimState = ETOI(plAnim::Pl_GameMenu_Unselected);

    return true;
}

void Player_GameMenu::Update(float timeDelta)
{
    if (_isPlayingIntro)
    {
        if (_bodyPlayer->Get_AnimationProgress() >= 0.95f &&
            _bodyPlayer->Set_AnimationByState(_pendingAnimState, 0.1f, true))
        {
            _isPlayingIntro = false;
            _pendingAnimState = 0;
        }
    }
}

void Player_GameMenu::LateUpdate(float timeDelta)
{
    
}

void Player_GameMenu::ExitInvoke()
{
    
}

void Player_GameMenu::Change_GameMenuAnimation(uint32 introAnim, uint32 idleAnim)
{
    if (_bodyPlayer->Get_Model()->Get_AnimIndexByState(introAnim)
        == _bodyPlayer->Get_Model()->Get_AnimIndexByState(idleAnim))
    {
        _bodyPlayer->Set_AnimationByState(idleAnim, 0.25f, true);
        _isPlayingIntro = false;
    }
    else
    {
        _bodyPlayer->Set_AnimationByState(introAnim, 0.25f, false);
        _pendingAnimState = idleAnim;
        _isPlayingIntro = true;
    }

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
