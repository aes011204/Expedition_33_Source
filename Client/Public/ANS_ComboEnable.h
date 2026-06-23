#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class ANS_ComboEnable final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_ComboEnable)

public:
    ANS_ComboEnable() = default;
    ~ANS_ComboEnable() override = default;

public:
    string Get_TypeName() const override { return "ANS_ComboEnable"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    void Set_ComboWindow(const AnimNotifyContext& context, bool enabled) const;
};

NS_END
