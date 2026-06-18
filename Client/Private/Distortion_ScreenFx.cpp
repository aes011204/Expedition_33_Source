#include "pch.h"
#include "Distortion_ScreenFx.h"
#include "Shader_CBuffer_Define.h"
#include "ShaderCom.h"
#include "GameInstance.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(Distortion_ScreenFx)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "Distortion";
    info.type = RegisteredType::ScreenFx;
    PROPERTY_FLOAT("Bulge Intensity", _originalDesc.bulgeIntensity, -5.f, 5.f, 0.01f);
    PROPERTY_FLOAT("Pull Intensity", _originalDesc.pullIntensity, 0.f, 5.f, 0.01f);
    PROPERTY_VEC2("Pull Direction", _originalDesc.pullDirection);
    PROPERTY_FLOAT("Duration", _originalDesc.duration, 0.01f, 30.f, 0.1f);
    PROPERTY_FLOAT("Delay", _originalDesc.delayTime, 0.f, 30.f, 0.1f);
    PROPERTY_BOOL("Is Reverse", _originalDesc.isReverse);
    PROPERTY_FLOAT("Elapsed Time", _elapsedTime, 0.f, 30.f, 0.01f);
    return true;
}

HRESULT Distortion_ScreenFx::Initialize(ScreenFxDesc* desc)
{
    if (desc)
    {
        if (FAILED(ScreenFx_Instance::Initialize(desc)))
            return E_FAIL;
    }

    _passType = 5;
    if (desc)
    {
        DistortionDesc* distDesc = static_cast<DistortionDesc*>(desc);
        _originalDesc = *distDesc; 
        
        _timeAcc = 0.0f;
        _duration = _originalDesc.duration + _originalDesc.delayTime;
        _currentBulge = _originalDesc.isReverse ? 0.f : _originalDesc.bulgeIntensity;
        _currentPull = _originalDesc.isReverse ? 0.f : _originalDesc.pullIntensity;
    }

    return S_OK;
}

void Distortion_ScreenFx::Update_ScreenFx(float timeDelta)
{
    _timeAcc += timeDelta;
    _elapsedTime = _timeAcc;
    _duration = _originalDesc.duration + _originalDesc.delayTime; // Update in case properties changed in editor
    
    if (_timeAcc < _originalDesc.delayTime)
    {
        _currentBulge = _originalDesc.isReverse ? 0.f : _originalDesc.bulgeIntensity;
        _currentPull = _originalDesc.isReverse ? 0.f : _originalDesc.pullIntensity;
        return;
    }

    float activeTime = _timeAcc - _originalDesc.delayTime;
    float ratio = std::clamp(activeTime / max(0.001f, _originalDesc.duration), 0.0f, 1.0f);

    if (!_originalDesc.isReverse)
        ratio = 1.0f - ratio;
        
    _currentBulge = _originalDesc.bulgeIntensity * ratio;
    _currentPull = _originalDesc.pullIntensity * ratio;

    float timeRemaining = _duration - _timeAcc;
    if (timeRemaining <= 0.5f && timeRemaining >= 0.1f)
    {
        GAME->Request_ScreenCapture(false);
    }
}

HRESULT Distortion_ScreenFx::Bind_FxConstantBuffer(class ShaderCom* shader)
{
    Engine::ScreenFxCB cbData{};
    
    cbData.bulgeIntensity = _currentBulge;
    cbData.pullIntensity = _currentPull;
    cbData.pullDirection = _originalDesc.pullDirection;
    
    return shader->Bind_CBufferData(cbData);
}

void Distortion_ScreenFx::Evaluate_At(float elapsedTime)
{
    _duration = _originalDesc.duration + _originalDesc.delayTime; // Sync duration for timeline

    if (elapsedTime < _originalDesc.delayTime)
    {
        _currentBulge = _originalDesc.isReverse ? 0.f : _originalDesc.bulgeIntensity;
        _currentPull = _originalDesc.isReverse ? 0.f : _originalDesc.pullIntensity;
        return;
    }

    float activeTime = elapsedTime - _originalDesc.delayTime;
    float durationToUse = max(0.001f, _originalDesc.duration);
    float ratio = std::clamp(activeTime / durationToUse, 0.f, 1.f);
    
    if (!_originalDesc.isReverse)
        ratio = 1.0f - ratio;

    _currentBulge = _originalDesc.bulgeIntensity * ratio;
    _currentPull = _originalDesc.pullIntensity * ratio;
}


Shared<Distortion_ScreenFx> Distortion_ScreenFx::Create(DistortionDesc* desc)
{
    auto instance = make_shared<Distortion_ScreenFx>();
    CHECK_FAILED(instance->Initialize(desc), nullptr);
    return instance;
}

NS_END

