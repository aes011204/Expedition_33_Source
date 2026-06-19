#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_SL_CameraReturn final : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_SL_CameraReturn)

public:
    string Get_TypeName() const override { return "AN_SL_CameraReturn"; }
    void Execute(const AnimNotifyContext& context) override;
};

NS_END
