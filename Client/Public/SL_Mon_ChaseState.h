#pragma once

#include "SL_Mon_State.h"

NS_BEGIN(Client)

class SL_Mon_ChaseState final : public SL_Mon_State
{
public:
    explicit SL_Mon_ChaseState(const Shared<SL_Dualliste>& owner);
    ~SL_Mon_ChaseState() override = default;

public:
    bool EnterInvoke() override;

    static Shared<SL_Mon_ChaseState> Create(const Shared<SL_Dualliste>& owner);
};

NS_END
