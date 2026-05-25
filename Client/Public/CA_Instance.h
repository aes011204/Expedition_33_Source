#pragma once

#include "ScreenFx_Instance.h"

NS_BEGIN(Client)

class CA_Instance final : public ScreenFx_Instance
{
public:

public:
    struct CADesc : public ScreenFx_Instance::ScreenFxDesc
    {
        float   maxIntensity{ 1.0f };
        float   duration{ 1.0f };
    };

private:
    CA_Instance(const CADesc& desc);
    ~CA_Instance() override = default;

public:
    bool Update_ScreenFx(float timeDelta) override;
    void Apply_To(struct Engine::ScreenFxCB& outParameters) override;

private:
    float   _maxIntensity{};
    float   _duration{};
    float   _timeAcc{};
    float   _currentIntensity{};

public:
    static Shared<CA_Instance> Create(const CADesc& desc);
};

NS_END
