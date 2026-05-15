#pragma once
#include "AnimNotifyState.h"

NS_BEGIN(Client)

class ANS_Parry : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_Parry)

public:
    ANS_Parry() = default;
    virtual ~ANS_Parry() = default;

public:
    string Get_TypeName() const override { return "ANS_Parry"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    float _testValue = 1.f;
};

NS_END
