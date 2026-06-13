#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class ANS_AnimSpeed : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_AnimSpeed)

public:
    string Get_TypeName() const override { return "ANS_AnimSpeed"; }

    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    float _frameSpeed = 2.f;          // 이 구간 동안 적용할 절대 애니메이션 배속
    float _restoreFrameSpeed = 1.f;   // ANS 시작 직전 배속. phase playRate 같은 기존 값 복구용
    bool _isApplied = false;          // Begin이 실제로 적용된 경우에만 End에서 복구한다.
};

NS_END
