#include "pch.h"
#include "Player_State.h"

#include "InputCom.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"

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

    _controllerCom = _owner.lock()->Get_Component<PlayerControllerCom>().get();
    if (nullptr == _controllerCom)
        MSG_BOX("[PLAYER STATE] : Failed to Begin Player State");

    _stateCom = _owner.lock()->Get_Component<PlayerStateMachineCom>().get();
    if (nullptr == _stateCom)
        MSG_BOX("[PLAYER STATE] : Failed to Begin Player State");

    _bodyPlayer = static_cast<Body_Player*>(_owner.lock()->Get_PartObject(L"Part_Body"));
}

bool Player_State::EnterInvoke()
{
    return State::EnterInvoke();
}

void Player_State::Update(float timeDelta)
{
    
}

void Player_State::LateUpdate(float timeDelta)
{
    
}

void Player_State::ExitInvoke()
{
    State::ExitInvoke();
}

bool Player_State::Has_MoveInput() const
{
    return _controllerCom->Has_MoveInput();
}

bool Player_State::Is_WalkToggled() const
{
    return _controllerCom->Is_WalkToggled();
}

const MovementCom::MovementInput& Player_State::Get_MovementInput() const
{
    return _controllerCom->Get_MovementInput();
}


void Player_State::Free()
{
    _owner.reset();
    _bodyPlayer = nullptr;

    State::Free();
}
