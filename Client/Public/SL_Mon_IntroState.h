#pragma once

#include "SL_Mon_State.h"

NS_BEGIN(Client)

class SL_Mon_IntroState final : public SL_Mon_State
{
public:
    enum class Phase
    {
        IntroSetup,
        IntroHold,
        IntroPlay,
        Dash,
        End
    };
public:
    explicit SL_Mon_IntroState(const Shared<SL_Dualliste>& owner);
    ~SL_Mon_IntroState() override = default;

public:
    bool EnterInvoke() override;
    void Play_CurrentPhase();
    void Update(float timeDelta) override;

public:
    Phase _phase = Phase::IntroSetup;
    float _holdElapsed = 0.f;
    static constexpr float kIntroHoldTime = 5.f;

    static Shared<SL_Mon_IntroState> Create(const Shared<SL_Dualliste>& owner);
};

NS_END
