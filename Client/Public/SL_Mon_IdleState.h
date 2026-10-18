#pragma once

#include "SL_Mon_State.h"

NS_BEGIN(Client)

class SL_Mon_IdleState final : public SL_Mon_State
{
public:
    explicit SL_Mon_IdleState(const Shared<SL_Dualliste>& owner);
    ~SL_Mon_IdleState() override = default;

public:
    bool EnterInvoke() override;

    static Shared<SL_Mon_IdleState> Create(const Shared<SL_Dualliste>& owner);
};

NS_END
