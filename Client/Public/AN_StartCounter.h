#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_StartCounter : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_StartCounter)

public:
    string Get_TypeName() const override { return "AN_StartCounter"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    float _slowTimeScale = 0.33f;     // Counter 시작 시 슬로우 배율
    float _slowDuration = 0.7f;       // 슬로우 지속시간
    float _counterPlayRate = 3.f;     // 플레이어 애니메이션 보정 속도

};

NS_END
