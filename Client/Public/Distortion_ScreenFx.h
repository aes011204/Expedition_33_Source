#pragma once
#include "ScreenFx_Instance.h"

NS_BEGIN(Client)

class Distortion_ScreenFx final : public ScreenFx_Instance
{
    GENERATED_SCREEN_FX(Distortion_ScreenFx)
public:
    struct DistortionDesc : public ScreenFxDesc
    {
        float bulgeIntensity{ 0.0f };
        float pullIntensity{ 0.0f };
        Vec2 pullDirection{ 0.0f, 0.0f };
        float delayTime{ 0.0f };
        bool isReverse{ false };
    };

public:
    Distortion_ScreenFx() { _passType = 5; }
    ~Distortion_ScreenFx() override = default;

public:
    HRESULT Initialize(ScreenFxDesc* desc) override;
    void Update_ScreenFx(float timeDelta) override;
    HRESULT Bind_FxConstantBuffer(class ShaderCom* shader) override;
    void Evaluate_At(float elapsedTime) override;

private:
    DistortionDesc _originalDesc{};
    float _timeAcc{};
    float _currentBulge{};
    float _currentPull{};

public:
    static Shared<Distortion_ScreenFx> Create(DistortionDesc* desc);
};
NS_END
