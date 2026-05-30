#include "pch.h"
#include "RadialBlur_ScreenFx.h"
#include "Shader_CBuffer_Define.h"
#include "ShaderCom.h"
#include <algorithm>

#include "ClientScreenFxManager.h"

NS_BEGIN(Client)

    IMPLEMENT_REFLECTION(RadialBlur_ScreenFx)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "Radial Blur (Action Cam)";
    info.type = RegisteredType::ScreenFx;

    PROPERTY_VEC2("Center", _originalDesc.center);
    PROPERTY_FLOAT("Intensity", _originalDesc.intensity, 0.f, 5.f, 0.01f);
    PROPERTY_FLOAT("Radius", _originalDesc.radius, 0.f, 2.f, 0.01f);
    PROPERTY_FLOAT("Duration", _originalDesc.duration, 0.01f, 10.f, 0.1f);
    PROPERTY_FLOAT("FadeOut Time", _originalDesc.fadeOutTime, 0.f, 10.f, 0.1f);
    PROPERTY_FLOAT("Elapsed Time", _elapsedTime, 0.f, 10.f, 0.01f);
    return true;
}

HRESULT RadialBlur_ScreenFx::Initialize(ScreenFxDesc* desc)
{
    if (desc)
    {
        if (FAILED(ScreenFx_Instance::Initialize(desc)))
            return E_FAIL;

        _passType = ETOI(ClientScreenFxManager::ScreenFx_Type::RadialBlur);
        RadialBlurDesc* blurDesc = static_cast<RadialBlurDesc*>(desc);
        _originalDesc = *blurDesc;
        _currentIntensity = _originalDesc.intensity;
    }
    return S_OK;
}

void RadialBlur_ScreenFx::Update_ScreenFx(float timeDelta)
{
    _elapsedTime += timeDelta;
    
    if (_originalDesc.fadeOutTime > 0.f)
    {
        float remainTime = _originalDesc.duration - _elapsedTime;
        if (remainTime <= _originalDesc.fadeOutTime && remainTime > 0.f)
        {
            float ratio = remainTime / _originalDesc.fadeOutTime;
            _currentIntensity = _originalDesc.intensity * ratio;
        }
        else if (remainTime <= 0.f)
        {
            _currentIntensity = 0.f;
        }
    }
}

HRESULT RadialBlur_ScreenFx::Bind_FxConstantBuffer(ShaderCom* shader)
{
    Engine::ScreenFxCB cbData{};

    cbData.radialBlurCenter = _originalDesc.center;
    cbData.radialBlurIntensity = _currentIntensity;
    cbData.radialBlurRadius = _originalDesc.radius;

    return shader->Bind_CBufferData(cbData);
}

void RadialBlur_ScreenFx::Evaluate_At(float elapsedTime)
{
    _elapsedTime = elapsedTime;

    if (_originalDesc.fadeOutTime > 0.0f)
    {
        float timeRemaining = _originalDesc.duration - _elapsedTime;
        if (timeRemaining <= _originalDesc.fadeOutTime && timeRemaining > 0.0f)
        {
            float ratio = timeRemaining / _originalDesc.fadeOutTime;
            _currentIntensity = _originalDesc.intensity * ratio;
        }
        else if (timeRemaining <= 0.0f)
        {
            _currentIntensity = 0.0f;
        }
        else
        {
            _currentIntensity = _originalDesc.intensity;
        }
    }
}

Shared<RadialBlur_ScreenFx> RadialBlur_ScreenFx::Create(RadialBlurDesc* desc)
{
    auto instance = make_shared<RadialBlur_ScreenFx>();
    CHECK_FAILED(instance->Initialize(desc), nullptr);
    return instance;
}

NS_END

