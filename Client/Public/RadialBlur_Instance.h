#pragma once

#include "ScreenFx_Instance.h"

NS_BEGIN(Client)

class RadialBlur_Instance final : public ScreenFx_Instance
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
    RadialBlur_Instance(const RadialBlurDesc& desc);
    ~RadialBlur_Instance() override = default;

public:
    bool Update_ScreenFx(float timeDelta) override;
    void Apply_To(struct Engine::ScreenFxCB& outParameters) override;

private:
    Vec2    _center{};
    float   _radius{};
    float   _maxIntensity{};
    
    float   _duration{};
    float   _timeAcc{};
    float   _currentIntensity{};

public:
    static Shared<RadialBlur_Instance> Create(const RadialBlurDesc& desc);
};

NS_END
