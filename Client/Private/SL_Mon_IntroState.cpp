#include "SL_Mon_IntroState.h"

#include "SL_Dualliste.h"
#include "SL_Mon_StateMachineCom.h"

NS_BEGIN(Client)

SL_Mon_IntroState::SL_Mon_IntroState(const Shared<SL_Dualliste>& owner)
    : SL_Mon_State(owner)
{
}

bool SL_Mon_IntroState::EnterInvoke()
{
    if (!SL_Mon_State::EnterInvoke())
        return false;

    _phase = Phase::IntroSetup;
    _holdElapsed = 0.f;
    Play_CurrentPhase();
    return true;
}

void SL_Mon_IntroState::Play_CurrentPhase()
{
    const Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return;

    switch (_phase)
    {
    case Phase::IntroSetup:
        //owner->Set_AnimPaused(false);
        owner->Play_Anim(SL_Dualliste::Anim::Intro, 0.0f, false, 0.0f);
        //owner->Refresh_AnimPose();
        //owner->Set_AnimPaused(true);
        break;
        break;

    case Phase::IntroHold:
        break;

    case Phase::IntroPlay:
        owner->Set_AnimPaused(false);
        break;

    case Phase::Dash:
        //owner->Set_AnimPaused(false);
        owner->Play_Anim(SL_Dualliste::Anim::DashIntro, 0.2f, false); // 1회만
        break;

    case Phase::End:
        break;
    }
}

void SL_Mon_IntroState::Update(float timeDelta)
{
    SL_Mon_State::Update(timeDelta);

    const Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr || _stateCom == nullptr)
        return;

    switch (_phase)
    {
    case Phase::IntroSetup:
        owner->Set_AnimPaused(true);
        _holdElapsed = 0.f;
        _phase = Phase::IntroHold;
        break;

    case Phase::IntroHold:
        _holdElapsed += timeDelta;
        if (_holdElapsed < kIntroHoldTime)
            return;

        _phase = Phase::IntroPlay;
        Play_CurrentPhase();
        break;

    case Phase::IntroPlay:
        if (!owner->Is_AnimFinished())
            return;

        _phase = Phase::Dash;
        Play_CurrentPhase();

        break;
    case Phase::Dash:
        if (!owner->Is_AnimFinished())
            return;

        _phase = Phase::End;
        owner->Set_IntroFin(true);
        _stateCom->Change_State(SL_MonState::Idle);
        break;
    case Phase::End:
        break;
    }
}

Shared<SL_Mon_IntroState> SL_Mon_IntroState::Create(const Shared<SL_Dualliste>& owner)
{
    auto instance = make_shared<SL_Mon_IntroState>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_MON_INTRO] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

NS_END
