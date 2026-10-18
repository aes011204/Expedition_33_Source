#pragma once

#include "SL_Mon_State.h"

NS_BEGIN(Client)

class SL_Mon_StunState final : public SL_Mon_State
{
public:
    enum class Phase
    {
        Enter,
        Loop,
        Exit
    };

public:
    explicit SL_Mon_StunState(const Shared<SL_Dualliste>& owner);
    ~SL_Mon_StunState() override = default;

public:
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    static Shared<SL_Mon_StunState> Create(const Shared<SL_Dualliste>& owner);

private:
    bool Play_PhaseAnim(Phase phase);

private:
    Phase _phase = Phase::Enter;
    float _loopElapsed = 0.f;
    bool _phaseChangeRequested = false;
};

NS_END
