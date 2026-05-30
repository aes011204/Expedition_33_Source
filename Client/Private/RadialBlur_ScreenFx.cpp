#include "pch.h"
#include "RadialBlur_ScreenFx.h"
#include "Shader_CBuffer_Define.h"
#include "ShaderCom.h"
#include <algorithm>

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(RadialBlur_ScreenFx)
{
    Vec2    _center{};
    float   _radius{};
    float   _maxIntensity{};
    float   _currentIntensity{};

    return true;
}

HRESULT RadialBlur_ScreenFx::Initialize(ScreenFxDesc* desc)
{
    if (FAILED(ScreenFx_Instance::Initialize(desc)))
        return E_FAIL;

    RadialBlurDesc* blurDesc = static_cast<RadialBlurDesc*>(desc);
    _center = blurDesc->center;
    _radius = blurDesc->radius;
    _maxIntensity = blurDesc->maxIntensity;
    _duration = blurDesc->duration;
    
    _elapsedTime = 0.0f;
    _currentIntensity = _maxIntensity;

    return S_OK;
}

void RadialBlur_ScreenFx::Update_ScreenFx(float timeDelta)
{
    _elapsedTime += timeDelta;
    float ratio = 1.0f - std::clamp(_elapsedTime / max(0.001f, _duration), 0.0f, 1.0f);
    _currentIntensity = _maxIntensity * ratio;
}

HRESULT RadialBlur_ScreenFx::Bind_FxConstantBuffer(ShaderCom* shader)
{
    ScreenFxCB cbData{};
    
    cbData.radialBlurCenter = _center;
    cbData.radialBlurRadius = _radius;
    cbData.radialBlurIntensity = _currentIntensity;
    
    return shader->Bind_CBufferData(cbData);
}

void RadialBlur_ScreenFx::Evaluate_At(float normalizedTime)
{
    
}

Shared<RadialBlur_ScreenFx> RadialBlur_ScreenFx::Create(RadialBlurDesc* desc)
{
    auto instance = make_shared<RadialBlur_ScreenFx>();
    CHECK_FAILED(instance->Initialize(desc), nullptr);
    return instance;
}

NS_END

