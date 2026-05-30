#include "pch.h"
#include "BattleTransition_ScreenFx.h"

#include "CA_ScreenFx.h"
#include "ClientScreenFxManager.h"
#include "GameInstance.h"
#include "Shader_CBuffer_Define.h"
#include "Renderer_Define.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(BattleTransition_ScreenFx)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "Battle Transition (Wipe)";
    info.type = RegisteredType::ScreenFx;

    PROPERTY_FLOAT("Duration", _originalDesc.duration, 0.1f, 10.f, 0.1f);
    PROPERTY_FLOAT("Edge Softness", _originalDesc.edgeSoftness, 0.0f, 1.f, 0.01f);
    PROPERTY_FLOAT("Max Radius", _originalDesc.maxRadius, 0.5f, 3.f, 0.1f);
    PROPERTY_FLOAT("Elapsed Time", _elapsedTime, 0.0f, 10.f, 0.01f);

    return true;
}

HRESULT BattleTransition_ScreenFx::Initialize(ScreenFxDesc* desc)
{
    if (FAILED(ScreenFx_Instance::Initialize(desc)))
        return E_FAIL;

    _passType = ETOI(ClientScreenFxManager::ScreenFx_Type::BattleTransition);
    if (desc)
    {
        BattleTransitionDesc* wipeDesc = static_cast<BattleTransitionDesc*>(desc);
        _originalDesc = *wipeDesc;
        _duration = _originalDesc.duration;
    }

    _currentRadius = 0.0f;
    return S_OK;
}

void BattleTransition_ScreenFx::Update_ScreenFx(float timeDelta)
{
    _elapsedTime += timeDelta;
    
    Evaluate_At(_elapsedTime);
}
void BattleTransition_ScreenFx::Evaluate_At(float elapsedTime)
{
    _elapsedTime = elapsedTime;
    _duration = _originalDesc.duration;

    float time = std::clamp(_elapsedTime / max(0.001f, _duration), 0.0f, 1.0f);

    float easeTime = time < 0.5f ? 2.f * time * time : 1.f - powf(-2.f * time + 2.f, 2.f) / 2.f;

    _currentRadius = _originalDesc.maxRadius * easeTime;
}

HRESULT BattleTransition_ScreenFx::Bind_FxConstantBuffer(ShaderCom* shader)
{
    Engine::ScreenFxCB cbData{};

    cbData.wipeRadius = _currentRadius;
    cbData.edgeSoftness = _originalDesc.edgeSoftness;
    cbData.transitionProgress = _elapsedTime / _duration;
    return shader->Bind_CBufferData(cbData);
}

HRESULT BattleTransition_ScreenFx::Bind_ShaderResources(const Shared<ShaderCom>& shader)
{
    return GAME->Bind_RT_ShaderResource(shader, "g_ScreenMaskTexture", RT::ScreenCapture);
}

Shared<BattleTransition_ScreenFx> BattleTransition_ScreenFx::Create(BattleTransitionDesc* desc)
{
    auto instance = make_shared<BattleTransition_ScreenFx>();
    if (FAILED(instance->Initialize(desc)))
        return nullptr;
    return instance;
}

NS_END
