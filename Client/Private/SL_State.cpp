#include "SL_State.h"

#include "Body_Player.h"
#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"

SL_State::SL_State(const Shared<SL_Maelle>& owner)
    : State()
    , _owner(owner)
{
}

HRESULT SL_State::Initialize()
{
    if (_owner.expired())
        return E_FAIL;

    return State::Initialize();
}

void SL_State::Begin_Play()
{
    Shared<SL_Maelle> owner = _owner.lock();
    if (owner == nullptr)
        return;

    _stateCom = owner->Get_Component<SL_StateMachineCom>().get();
    _body = owner->Get_Body();
}

bool SL_State::EnterInvoke()
{
    return State::EnterInvoke();
}

void SL_State::Update(float timeDelta)
{
    State::Update(timeDelta);
}

void SL_State::LateUpdate(float timeDelta)
{
    State::LateUpdate(timeDelta);
}

void SL_State::ExitInvoke()
{
    State::ExitInvoke();
}

bool SL_State::Request_Dodge(const SL_MoveRequest& request)
{
    return false;
}

bool SL_State::Play_Anim(SoulLikeAnim anim, float blendDuration, bool loop, float startTimeSec)
{
    if (_body == nullptr)
        return false;

    if (startTimeSec > 0.f)
        return _body->Set_AnimationByStateTimeSec(ETOI(anim), blendDuration, loop, startTimeSec);

    return _body->Set_AnimationByState(ETOI(anim), blendDuration, loop);
}

bool SL_State::Is_AnimFinished() const
{
    return _body != nullptr && _body->Is_AnimationFinished();
}

void SL_State::Stop_Movement()
{
    Shared<SL_Maelle> owner = _owner.lock();
    if (owner)
        owner->Stop_Move();
}

void SL_State::Free()
{
    _owner.reset();
    _stateCom = nullptr;
    _body = nullptr;

    State::Free();
}
