#pragma once
#include "ScreenFx_Instance.h"

NS_BEGIN(Engine)
class ShaderCom;
class TransformCom;
NS_END

NS_BEGIN(Client)

class RadialBlur_ScreenFx final : public ScreenFx_Instance
{
    GENERATED_BASE_TYPE_BODY(RadialBlur_ScreenFx)
public:
    struct RadialBlurDesc : public ScreenFxDesc
    {
        Vec2 center{ 0.5f, 0.5f };
        float intensity{ 0.f };
        float radius{ 0.f };
        float fadeOutTime{ 0.f };
    };

public:
    RadialBlur_ScreenFx() = default;
    ~RadialBlur_ScreenFx() override = default;

public:
    HRESULT Initialize(ScreenFxDesc* desc) override;
    void Update_ScreenFx(float timeDelta) override;
    HRESULT Bind_FxConstantBuffer(ShaderCom* shader) override;
    HRESULT Bind_ShaderResources(const Shared<ShaderCom>& shader) override;
    void Evaluate_At(float elapsedTime) override;

private:
    Shared<TransformCom> Get_CurrentTarget();
    void Compute_LeaderHeroPosToProjection(const Shared<TransformCom>& targetTransform);

private:
    RadialBlurDesc _originalDesc{};
    float _currentIntensity{};
    float _targetIntensity{ 0.0f };
    Vec3 _prevLookDir{};
    float _requireSpeed = 0.1f;

public:
    static Shared<RadialBlur_ScreenFx> Create(RadialBlurDesc* desc);
};

NS_END
