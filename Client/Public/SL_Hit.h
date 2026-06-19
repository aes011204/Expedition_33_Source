#pragma once

#include "SL_State.h"

NS_BEGIN(Client)

class SL_Hit final : public SL_State
{
public:
    explicit SL_Hit(const Shared<SL_Maelle>& owner);
    ~SL_Hit() override = default;

public:
    bool EnterInvoke() override;
    void Update(float timeDelta) override;

public:
    static Shared<SL_Hit> Create(const Shared<SL_Maelle>& owner);
};

NS_END
