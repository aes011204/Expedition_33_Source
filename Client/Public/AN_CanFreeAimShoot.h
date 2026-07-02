#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_CanFreeAimShoot final : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_CanFreeAimShoot)

public:
    AN_CanFreeAimShoot() = default;
    ~AN_CanFreeAimShoot() override = default;

public:
    void Execute(const AnimNotifyContext& context) override;
    string Get_TypeName() const override { return "AN_CanFreeAimShoot"; }
};

NS_END
