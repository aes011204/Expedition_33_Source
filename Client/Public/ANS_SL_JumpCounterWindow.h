#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class ANS_SL_JumpCounterWindow final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_SL_JumpCounterWindow)

public:
    ANS_SL_JumpCounterWindow() = default;
    ~ANS_SL_JumpCounterWindow() override = default;

public:
    string Get_TypeName() const override { return "ANS_SL_JumpCounterWindow"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;
};

NS_END
