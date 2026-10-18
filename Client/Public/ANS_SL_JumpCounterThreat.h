#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class SL_Base;

class ANS_SL_JumpCounterThreat final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_SL_JumpCounterThreat)

public:
    ANS_SL_JumpCounterThreat() = default;
    ~ANS_SL_JumpCounterThreat() override = default;

public:
    string Get_TypeName() const override { return "ANS_SL_JumpCounterThreat"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    Shared<SL_Base> Resolve_Attacker(GameObject* notifyOwner) const;
};

NS_END
