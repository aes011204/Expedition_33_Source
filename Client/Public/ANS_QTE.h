#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class ANS_QTE : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_QTE)

public:
    ANS_QTE() = default;
    virtual ~ANS_QTE() = default;

public:
    string Get_TypeName() const override { return "ANS_QTE"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    float _testValue = 1.f;
};

NS_END
