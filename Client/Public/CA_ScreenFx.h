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

public:
    CA_ScreenFx() = default;
    ~CA_ScreenFx() override = default;

public:
    HRESULT Initialize(ScreenFxDesc* desc) override;
    void Update_ScreenFx(float timeDelta) override;
    HRESULT Bind_FxConstantBuffer(class ShaderCom* shader) override;

private:
    float   _maxIntensity{};
    float   _duration{};
    float   _timeAcc{};
    float   _currentIntensity{};

public:
    static Shared<CA_ScreenFx> Create(CADesc* desc);
};

NS_END
