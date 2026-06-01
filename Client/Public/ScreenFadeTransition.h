#pragma once

#include "Base.h"

NS_BEGIN(Client)

class UI_ScreenFade;

class ScreenFadeTransition final : public Base
{
public:
    ScreenFadeTransition() = default;
    ~ScreenFadeTransition() override;

public:
    HRESULT Initialize();
    void    Update(float TimeDelta);

    bool    Request_Transition(
        const function<void()>& CoveredAction,
        float FadeOutDuration = 0.35f,
        float FadeInDuration = 0.35f);

    bool    Is_Playing() const { return _state != State::Idle; }

private:
    enum class State
    {
        Idle,
        FadeOut,
        FadeIn,
    };

private:
    void    Begin_Fade(State NextState, float TargetAlpha, float Duration);
    void    Update_Fade(float TimeDelta);
    void    Complete_FadeOut();
    void    Complete_FadeIn();

private:
    Unique<UI_ScreenFade> _screenFade{};
    function<void()>      _coveredAction{};

    State _state = State::Idle;
    float _elapsed = 0.f;
    float _duration = 0.f;
    float _startAlpha = 0.f;
    float _targetAlpha = 0.f;
    float _fadeInDuration = 0.35f;

public:
    static Unique<ScreenFadeTransition> Create();
    void Free() override;
};

NS_END
