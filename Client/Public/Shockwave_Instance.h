#pragma once

#include "ScreenFx_Instance.h"

NS_BEGIN(Client)

class Shockwave_Instance final : public ScreenFx_Instance
{
public:
    struct ShockwaveDesc : public ScreenFx_Instance::ScreenFxDesc
    {
        Vec2  center{ 0.5f, 0.5f };
        float maxRadius{ 1.0f };
        float maxBlurIntensity{ 0.5f };
        float maxCAIntensity{ 0.1f };
        float maxBulgeIntensity{ 1.0f };
    };

private:
    Shockwave_Instance(const ShockwaveDesc& desc);
    ~Shockwave_Instance() override = default;

public:
    void Update(float timeDelta) override;
    HRESULT Bind_FxConstantBuffer(class ShaderCom* shader) override;

private:
    ShockwaveDesc _desc{};
    
    // 현재 프레임에 계산된 값들
    float _currentRadius{};
    float _currentBlur{};
    float _currentCA{};
    float _currentBulge{};

public:
    static Shared<Shockwave_Instance> Create(const ShockwaveDesc& desc);
};

NS_END
