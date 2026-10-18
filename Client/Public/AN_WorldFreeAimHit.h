#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_WorldFreeAimHit final : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_WorldFreeAimHit)

public:
    AN_WorldFreeAimHit() = default;
    ~AN_WorldFreeAimHit() override = default;

public:
    void Execute(const AnimNotifyContext& context) override;
    string Get_TypeName() const override { return "AN_WorldFreeAimHit"; }
    
};

NS_END
