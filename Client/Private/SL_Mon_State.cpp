#include "SL_Mon_State.h"

#include "SL_Dualliste.h"
#include "SL_Mon_StateMachineCom.h"

NS_BEGIN(Client)

SL_Mon_State::SL_Mon_State(const Shared<SL_Dualliste>& owner)
    : State()
    , _owner(owner)
{
}

HRESULT SL_Mon_State::Initialize()
{
    if (_owner.expired())
        return E_FAIL;

    return State::Initialize();
}

void SL_Mon_State::Begin_Play()
{
    Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return;

    _stateCom = owner->Get_Component<SL_Mon_StateMachineCom>().get();
}

bool SL_Mon_State::EnterInvoke()
{
    return State::EnterInvoke();
}

void SL_Mon_State::Update(float timeDelta)
{
    State::Update(timeDelta);
}

void SL_Mon_State::LateUpdate(float timeDelta)
{
    State::LateUpdate(timeDelta);
}

void SL_Mon_State::ExitInvoke()
{
    State::ExitInvoke();
}

void SL_Mon_State::Free()
{
    _owner.reset();
    _stateCom = nullptr;

    State::Free();
}

NS_END
