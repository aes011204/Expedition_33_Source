#pragma once
#include "ScreenFx_Instance.h"

NS_BEGIN(Client)

class Distortion_ScreenFx final : public ScreenFx_Instance
{
public:
    struct DistortionDesc : public ScreenFxDesc
    {
        float   bulgeIntensity{ 0.0f };   // 양수: 볼록, 음수: 오목
        float   pullIntensity{ 0.0f };    // 찢어지는 세기
        Vec2    pullDirection{ 0.0f, 0.0f }; // 방향 (예: -1, 1 좌상단)
        float   duration{ 1.0f };
    };

private:
    Distortion_ScreenFx();
    ~Distortion_ScreenFx() override = default;

public:
    HRESULT Initialize(ScreenFxDesc* desc) override;
    HRESULT Bind_ShaderResources(ShaderCom* shader) override;

private:
    DistortionDesc _originalDesc{};
    float _timeAcc{};
    float _currentBulge{};
    float _currentPull{};

public:
    static Shared<Distortion_ScreenFx> Create(ScreenFxDesc* desc);
};
NS_END
