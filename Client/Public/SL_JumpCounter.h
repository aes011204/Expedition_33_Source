#pragma once

#include "SL_State.h"

NS_BEGIN(Client)

class SL_JumpCounter final : public SL_State
{
public:
    explicit SL_JumpCounter(const Shared<SL_Maelle>& owner);
    ~SL_JumpCounter() override = default;

public:
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void ExitInvoke() override;

public:
    static Shared<SL_JumpCounter> Create(const Shared<SL_Maelle>& owner);
};

NS_END
