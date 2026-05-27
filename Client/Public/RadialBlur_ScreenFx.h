#pragma once

#include "ScreenFx_Instance.h"

NS_BEGIN(Client)

class RadialBlur_ScreenFx final : public ScreenFx_Instance
{
public:
    struct RadialBlurDesc : public ScreenFx_Instance::ScreenFxDesc
    {
        Vec2    center{ 0.5f, 0.5f };
        float   radius{ 1.0f };
        float   maxIntensity{ 1.0f };
        float   duration{ 1.0f };
    };

private:
    RadialBlur_ScreenFx(const RadialBlurDesc& desc);
    ~RadialBlur_ScreenFx() override = default;

private:
    Vec2    _center{};
    float   _radius{};
    float   _maxIntensity{};
    
    float   _duration{};
    float   _timeAcc{};
    float   _currentIntensity{};

public:
    static Shared<RadialBlur_ScreenFx> Create(const RadialBlurDesc& desc);
};

NS_END
