#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_CounterReturnAll : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_CounterReturnAll)

public:
    string Get_TypeName() const override { return "AN_CounterReturnAll"; }
    void Execute(const AnimNotifyContext& context) override;
};

NS_END
