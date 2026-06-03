#pragma once
#include "AnimNotify.h"

NS_BEGIN(Client)
class AN_CameraShake : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_CameraShake)

public:
    AN_CameraShake() = default;
    virtual ~AN_CameraShake() = default;

public:
    string Get_TypeName() const override { return "AN_CameraShake";}
    void Execute(const AnimNotifyContext& context) override;

private:
    float _amplitude     {};
    float _duration      {};
    float _frequencyX = 32.f;
    float _frequencyY = 47.f;
    float _decayRateX = 0.f;
    float _decayRateY = 8.f;
    float _verticalRatio = 0.8f;
    float _strengthScale = 0.05f;
};



NS_END
