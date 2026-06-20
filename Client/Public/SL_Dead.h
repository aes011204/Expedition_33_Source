#pragma once

#include "SL_State.h"

NS_BEGIN(Client)

class SL_Dead final : public SL_State
{
public:
    explicit SL_Dead(const Shared<SL_Maelle>& owner);
    ~SL_Dead() override = default;

public:
    bool EnterInvoke() override;

public:
    static Shared<SL_Dead> Create(const Shared<SL_Maelle>& owner);
};

NS_END
