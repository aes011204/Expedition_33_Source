#include "SL_State.h"

SL_State::SL_State(const Shared<SL_Maelle>& owner)
{
}

HRESULT SL_State::Initialize()
{
    return State::Initialize();
}

void SL_State::Begin_Play()
{
    State::Begin_Play();
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

bool SL_State::Play_Anim(SoulLikeAnim anim, float blendDuration, bool loop)
{
}

bool SL_State::Play_AnimationKey(const string& animationKey, bool loop)
{
}

bool SL_State::Is_AnimFinished() const
{
}

void SL_State::Stop_Movement()
{
}

void SL_State::Free()
{
    State::Free();
}
