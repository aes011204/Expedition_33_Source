#pragma once

#include "ScreenFx_Instance.h"

NS_BEGIN(Client)

class CA_ScreenFx final : public ScreenFx_Instance
{
public:

public:
    struct CADesc : public ScreenFx_Instance::ScreenFxDesc
    {
        float   maxIntensity{ 1.0f };
        float   duration{ 1.0f };
    };

private:
    CA_ScreenFx(const CADesc& desc);
    ~CA_ScreenFx() override = default;

public:

private:
    float   _maxIntensity{};
    float   _duration{};
    float   _timeAcc{};
    float   _currentIntensity{};

public:
    static Shared<CA_ScreenFx> Create(const CADesc& desc);
};

NS_END
