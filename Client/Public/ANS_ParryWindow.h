#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class SL_Maelle;

class ANS_ParryWindow final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_ParryWindow)

public:
    ANS_ParryWindow() = default;
    ~ANS_ParryWindow() override = default;

public:
    string Get_TypeName() const override { return "ANS_ParryWindow"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    Shared<SL_Maelle> Resolve_Maelle(GameObject* notifyOwner) const;
};

NS_END
