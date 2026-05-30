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
    Shared<RadialBlur_ScreenFx> blurFx = _activeActionBlur.lock();

    // 1. 이미 블러 이펙트가 화면에 살아있다면? -> "수명 연장 및 강도 갱신"
    if (blurFx && !blurFx->Check_EndCondition())
    {
        blurFx->Refresh_ActionBlur(intensity, fadeOutTime);
    }
    // 2. 블러 이펙트가 없거나 이미 끝났다면? -> "새로 생성"
    else
    {
        RadialBlur_ScreenFx::RadialBlurDesc desc{};
        desc.center = Vec2(0.5f, 0.5f);
        desc.intensity = intensity;

        // 만약 정말 화면 전체에 블러를 골고루 먹이고 싶다면 radius를 0.0f로 주세요.
        // 외곽에만 먹이고 싶다면 0.7f 정도 유지.
        desc.radius = 0.0f;

        desc.duration = fadeOutTime;
        desc.fadeOutTime = fadeOutTime;

        auto newBlur = RadialBlur_ScreenFx::Create(&desc);
        if (newBlur)
        {
            GAME->Get_ScreenFxManager()->Play_ScreenEffect(newBlur);
            _activeActionBlur = newBlur; // 매니저가 이펙트를 기억함
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

