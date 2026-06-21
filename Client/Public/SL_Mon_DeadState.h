#pragma once

#include "SL_Mon_State.h"

NS_BEGIN(Client)

class SL_Mon_DeadState final : public SL_Mon_State
{
public:
    explicit SL_Mon_DeadState(const Shared<SL_Dualliste>& owner);
    ~SL_Mon_DeadState() override = default;

public:
    bool EnterInvoke() override;
    void Update(float timeDelta) override;

    static Shared<SL_Mon_DeadState> Create(const Shared<SL_Dualliste>& owner);
};

NS_END
