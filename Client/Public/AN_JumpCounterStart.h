#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_JumpCounterStart : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_JumpCounterStart)

public:
    string Get_TypeName() const override { return "AN_JumpCounterStart"; }
    void Execute(const AnimNotifyContext& context) override;
};

NS_END
