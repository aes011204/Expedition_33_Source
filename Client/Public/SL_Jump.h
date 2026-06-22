#pragma once

#include "SL_State.h"

NS_BEGIN(Client)

class SL_Jump final : public SL_State
{
public:
    explicit SL_Jump(const Shared<SL_Maelle>& owner);
    ~SL_Jump() override = default;

public:
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void ExitInvoke() override;
    bool Request_JumpCounter() override;

public:
    static Shared<SL_Jump> Create(const Shared<SL_Maelle>& owner);
};

NS_END
