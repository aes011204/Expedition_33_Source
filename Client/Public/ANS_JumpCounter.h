#pragma once
#include "AnimNotifyState.h"

NS_BEGIN(Client)

class ANS_JumpCounter : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_JumpCounter)

public:
    ANS_JumpCounter() = default;
    ~ANS_JumpCounter() override = default;

public:
    string Get_TypeName() const override { return "ANS_JumpCounter"; }
    float Get_JumpCounterWorldTimeScale() const { return _jumpCounterWorldTimeScale; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    float _jumpCounterWorldTimeScale = 0.15f;
    bool _startedCounter = false;
};

NS_END
