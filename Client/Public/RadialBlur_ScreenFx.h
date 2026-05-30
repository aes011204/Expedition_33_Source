#pragma once
#include "ScreenFx_Instance.h"

NS_BEGIN(Engine)
class ShaderCom;
NS_END

NS_BEGIN(Client)

class RadialBlur_ScreenFx final : public ScreenFx_Instance
{
    GENERATED_BASE_TYPE_BODY(RadialBlur_ScreenFx)
public:
    struct RadialBlurDesc : public ScreenFxDesc
    {
        Vec2    center{ 0.5f, 0.5f };
        float   radius{ 1.0f };
        float   maxIntensity{ 1.0f };
        float   duration{ 1.0f };
    };

public:
    RadialBlur_ScreenFx() = default;
    ~RadialBlur_ScreenFx() override = default;

public:
    HRESULT Initialize(ScreenFxDesc* desc) override;
    void Update_ScreenFx(float timeDelta) override;
    HRESULT Bind_FxConstantBuffer(ShaderCom* shader) override;
    void Evaluate_At(float normalizedTime) override;

private:
    Vec2    _center{};
    float   _radius{};
    float   _maxIntensity{};
    float   _currentIntensity{};

public:
    static Shared<RadialBlur_ScreenFx> Create(RadialBlurDesc* desc);
};

NS_END
