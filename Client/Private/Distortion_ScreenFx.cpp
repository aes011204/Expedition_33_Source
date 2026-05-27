#include "pch.h"
#include "Distortion_ScreenFx.h"
#include "Shader_CBuffer_Define.h"
#include "ShaderCom.h"
#include <algorithm>

NS_BEGIN(Client)

HRESULT Distortion_ScreenFx::Initialize(ScreenFxDesc* desc)
{
    if (FAILED(ScreenFx_Instance::Initialize(desc)))
        return E_FAIL;

    DistortionDesc* distDesc = static_cast<DistortionDesc*>(desc);
    _originalDesc = *distDesc; 
    
    _timeAcc = 0.0f;
    _currentBulge = distDesc->bulgeIntensity;
    _currentPull = distDesc->pullIntensity;

    return S_OK;
}

void Distortion_ScreenFx::Update_ScreenFx(float timeDelta)
{
    _timeAcc += timeDelta;
    float ratio = 1.0f - std::clamp(_timeAcc / max(0.001f, _originalDesc.duration), 0.0f, 1.0f);
    
    _currentBulge = _originalDesc.bulgeIntensity * ratio;
    _currentPull = _originalDesc.pullIntensity * ratio;
}

HRESULT Distortion_ScreenFx::Bind_FxConstantBuffer(class ShaderCom* shader)
{
    Engine::ScreenFxCB cbData{};
    
    cbData.bulgeIntensity = _currentBulge;
    cbData.pullIntensity = _currentPull;
    cbData.pullDirection = _originalDesc.pullDirection;
    
    return shader->Bind_CBufferData(cbData);
}


Shared<Distortion_ScreenFx> Distortion_ScreenFx::Create(DistortionDesc* desc)
{
    auto instance = make_shared<Distortion_ScreenFx>();
    CHECK_FAILED(instance->Initialize(desc), nullptr);
    return instance;
}

NS_END

