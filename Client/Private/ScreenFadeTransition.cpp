#include "pch.h"
#include "ScreenFadeTransition.h"

#include "GameInstance.h"
#include "UI_ScreenFade.h"

ScreenFadeTransition::~ScreenFadeTransition() = default;

HRESULT ScreenFadeTransition::Initialize()
{
    _screenFade = UI_ScreenFade::Create();
    CHECK_NULL(_screenFade, E_FAIL);

    return S_OK;
}

void ScreenFadeTransition::Update(float TimeDelta)
{
    if (_screenFade)
        _screenFade->Update(GAME->Get_UnscaledTimeDelta());

    Update_Fade(GAME->Get_UnscaledTimeDelta());
}

bool ScreenFadeTransition::Request_Transition(
    const function<void()>& CoveredAction,
    float FadeOutDuration,
    float FadeInDuration)
{
    if (Is_Playing())
        return false;

    if (_screenFade == nullptr)
    {
        if (CoveredAction)
            CoveredAction();
        return true;
    }

    _coveredAction = CoveredAction;
    _fadeInDuration = max(0.f, FadeInDuration);
    Begin_Fade(State::FadeOut, 1.f, max(0.f, FadeOutDuration));

    return true;
}

void ScreenFadeTransition::Begin_Fade(State NextState, float TargetAlpha, float Duration)
{
    _state = NextState;
    _elapsed = 0.f;
    _duration = Duration;
    _startAlpha = _screenFade ? _screenFade->Get_FadeAlpha() : 0.f;
    _targetAlpha = clamp(TargetAlpha, 0.f, 1.f);

    if (_duration <= 0.f)
    {
        if (_screenFade)
            _screenFade->Set_FadeAlpha(_targetAlpha);

        if (_state == State::FadeOut)
            Complete_FadeOut();
        else if (_state == State::FadeIn)
            Complete_FadeIn();
    }
}

void ScreenFadeTransition::Update_Fade(float TimeDelta)
{
    if (_state == State::Idle || _screenFade == nullptr)
        return;

    if (_duration <= 0.f)
        return;

    _elapsed += TimeDelta;
    const float ratio = clamp(_elapsed / _duration, 0.f, 1.f);
    const float alpha = std::lerp(_startAlpha, _targetAlpha, ratio);
    _screenFade->Set_FadeAlpha(alpha);

    if (ratio < 1.f)
        return;

    if (_state == State::FadeOut)
        Complete_FadeOut();
    else if (_state == State::FadeIn)
        Complete_FadeIn();
}

void ScreenFadeTransition::Complete_FadeOut()
{
    if (_screenFade)
        _screenFade->Set_FadeAlpha(1.f);

    function<void()> action = std::move(_coveredAction);
    _coveredAction = nullptr;

    if (action)
        action();

    Begin_Fade(State::FadeIn, 0.f, _fadeInDuration);
}

void ScreenFadeTransition::Complete_FadeIn()
{
    if (_screenFade)
        _screenFade->Set_FadeAlpha(0.f);

    _state = State::Idle;
    _elapsed = 0.f;
    _duration = 0.f;
    _startAlpha = 0.f;
    _targetAlpha = 0.f;
}

Unique<ScreenFadeTransition> ScreenFadeTransition::Create()
{
    auto instance = make_unique<ScreenFadeTransition>();

    if (FAILED(instance->Initialize()))
    {
        instance->Free();
        return nullptr;
    }

    return instance;
}

void ScreenFadeTransition::Free()
{
    if (_screenFade)
    {
        _screenFade->Free();
        _screenFade.reset();
    }

    _coveredAction = nullptr;
    __super::Free();
}
