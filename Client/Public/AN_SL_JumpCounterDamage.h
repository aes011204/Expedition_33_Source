#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_SL_JumpCounterDamage final : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_SL_JumpCounterDamage)

public:
    AN_SL_JumpCounterDamage() = default;
    ~AN_SL_JumpCounterDamage() override = default;

public:
    string Get_TypeName() const override { return "AN_SL_JumpCounterDamage"; }
    void Execute(const AnimNotifyContext& context) override;
};

NS_END
