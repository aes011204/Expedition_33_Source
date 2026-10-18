#include "pch.h"
#include "CA_ScreenFx.h"
#include "Shader_CBuffer_Define.h"
#include "ShaderCom.h"
#include <algorithm>

NS_BEGIN(Client)

HRESULT CA_ScreenFx::Initialize(ScreenFxDesc* desc)
{
    if (FAILED(ScreenFx_Instance::Initialize(desc)))
        return E_FAIL;

    CADesc* caDesc = static_cast<CADesc*>(desc);
    _maxIntensity = caDesc->maxIntensity;
    _duration = caDesc->duration;
    
    _timeAcc = 0.0f;
    _currentIntensity = _maxIntensity;

    return S_OK;
}

void CA_ScreenFx::Update_ScreenFx(float timeDelta)
{
    _timeAcc += timeDelta;
    _elapsedTime = _timeAcc;
    float ratio = 1.0f - std::clamp(_timeAcc / max(0.001f, _duration), 0.0f, 1.0f);
    _currentIntensity = _maxIntensity * ratio;
}

HRESULT CA_ScreenFx::Bind_FxConstantBuffer(class ShaderCom* shader)
{
    Engine::ScreenFxCB cbData{};
    
    cbData.chromaticAberrationIntensity = _currentIntensity;
    
    return shader->Bind_CBufferData(cbData);
}

Shared<CA_ScreenFx> CA_ScreenFx::Create(CADesc* desc)
{
    auto instance = make_shared<CA_ScreenFx>();
    CHECK_FAILED(instance->Initialize(desc), nullptr);
    return instance;
}

NS_END

