#include "pch.h"
#include "Battle_State.h"
#include "BattleController.h"
#include "BattleStateMachine.h"

NS_BEGIN(Client)

Battle_State::Battle_State(const Shared<BattleController>& owner)
    : State{}
{
    _owner = owner;
}

Battle_State::~Battle_State()
{
}

HRESULT Battle_State::Initialize()
{
    if (_owner.expired())
        return E_FAIL;

    return State::Initialize();
}

void Battle_State::Begin_Play()
{
    auto owner = _owner.lock();
    if (!owner)
        return;

    _battleFSM = owner->Get_BattleFSM().get();

    State::Begin_Play();
}

bool Battle_State::EnterInvoke()
{
    return State::EnterInvoke();
}

void Battle_State::Update(float timeDelta)
{
}

void Battle_State::LateUpdate(float timeDelta)
{
}

void Battle_State::ExitInvoke()
{
    State::ExitInvoke();
}

void Battle_State::Free()
{
    _owner.reset();
    _battleFSM = nullptr;
    State::Free();
}

NS_END
