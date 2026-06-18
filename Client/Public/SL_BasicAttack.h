#pragma once

#include "SL_State.h"

NS_BEGIN(Client)

class SL_BasicAttack final : public SL_State
{
public:
    explicit SL_BasicAttack(const Shared<SL_Maelle>& owner);
    ~SL_BasicAttack() override = default;

public:
    bool EnterInvoke() override;
    void Update(float timeDelta) override;

public:
    static Shared<SL_BasicAttack> Create(const Shared<SL_Maelle>& owner);
};

NS_END
