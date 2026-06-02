#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class ANS_CounterTimeScale : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_CounterTimeScale)

public:
    string Get_TypeName() const override { return "ANS_CounterTimeScale"; }

    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    float _worldTimeScale = 1.f;      // 이 구간 동안 적용할 전투 월드 속도
    float _counterFrameSpeed = 1.f;   // 이 구간 동안 카운터 애니메이션만 보정할 속도
};

NS_END
