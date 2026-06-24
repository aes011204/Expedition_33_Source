#pragma once

#include "SL_Mon_State.h"

NS_BEGIN(Client)

class SL_Mon_PhaseChangeState final : public SL_Mon_State
{
public:
    enum class Phase
    {
        StunStart,
        StunEnd,
        PhaseChange,
        End
    };

public:
    explicit SL_Mon_PhaseChangeState(const Shared<SL_Dualliste>& owner);
    ~SL_Mon_PhaseChangeState() override = default;

public:
    bool EnterInvoke() override;
    void Update(float timeDelta) override;

private:
    void Play_CurrentPhase();

private:
    Phase _phase = Phase::StunStart;
    bool _hasStarted = false;

public:
    static Shared<SL_Mon_PhaseChangeState> Create(const Shared<SL_Dualliste>& owner);
};

NS_END
