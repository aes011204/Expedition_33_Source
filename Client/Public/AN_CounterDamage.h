#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_CounterDamage : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_CounterDamage)

public:
    string Get_TypeName() const override { return "AN_CounterDamage"; }
    void Execute(const AnimNotifyContext& context) override;

};

NS_END
