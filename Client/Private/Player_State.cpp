#include "pch.h"
#include "Player_State.h"

#include "Player.h"
#include "Body_Player.h"

Player_State::Player_State(const Shared<Player>& owner)
    : State{}
{
    _owner = owner;
}

HRESULT Player_State::Initialize()
{
    if (_owner.expired()) return E_FAIL;

    return State::Initialize();
}

void Player_State::Begin_Play()
{
    if (_owner.expired()) return;

    _bodyPlayer = static_cast<Body_Player*>(_owner.lock()->Get_PartObject(L"Part_Body"));
}

bool Player_State::EnterInvoke()
{
    return State::EnterInvoke();
}

void Player_State::Update(float timeDelta)
{
    State::Update(timeDelta);
}

void Player_State::LateUpdate(float timeDelta)
{
    State::LateUpdate(timeDelta);
}

void Player_State::ExitInvoke()
{
    State::ExitInvoke();
}


void Player_State::Free()
{
    _owner.reset();
    _bodyPlayer = nullptr;

    State::Free();
}
