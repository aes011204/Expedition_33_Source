#include "pch.h"
#include "BattleTransition_ScreenFx.h"

#include <Camera.h>

#include "ClientScreenFxManager.h"
#include "EffectInstancePool.h"
#include "GameInstance.h"
#include "Shader_CBuffer_Define.h"
#include "Renderer_Define.h"
#include "ClientInstance.h"

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

BattleTransition_ScreenFx::~BattleTransition_ScreenFx()
{
    Release_RoseFlare();
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
    _smudgeMaskSRV = GAME->Get_SRV("T_Noise02");

    return S_OK;
}

void BattleTransition_ScreenFx::Update_ScreenFx(float timeDelta)
{
    _elapsedTime += timeDelta;
    
    Evaluate_At(_elapsedTime);

    if (_isRoseFlare == false)
    {
        RoseFlare(); 
        _isRoseFlare = true;
    }

    if (_roseFlareHandle.effectObject == nullptr) return;
    
    if (auto mainCamera = GAME->Get_ActiveCamera())
    {
        auto camTransform = mainCamera->Get_Transform();
        auto effectTransform = _roseFlareHandle.effectObject->Get_Transform();

        if (camTransform != nullptr && effectTransform != nullptr)
        {
            Vec3 pos = camTransform->Get_WorldForward() * 3.f + camTransform->Get_WorldPosition();

            effectTransform->Set_WorldPosition(pos);
        }
    }
}
void BattleTransition_ScreenFx::Evaluate_At(float elapsedTime)
{
    _elapsedTime = elapsedTime;
    _duration = _originalDesc.duration;

    float time = std::clamp(_elapsedTime / max(0.001f, _duration), 0.0f, 1.0f);

	float easeTime = powf(time, 4.f);

    _currentRadius = _originalDesc.maxRadius * easeTime;
}

HRESULT BattleTransition_ScreenFx::Bind_FxConstantBuffer(ShaderCom* shader)
{
    Engine::ScreenFxCB cbData{};

    cbData.wipeRadius = _currentRadius;
    cbData.edgeSoftness = _originalDesc.edgeSoftness;
    cbData.transitionProgress = _elapsedTime / _duration;

    cbData.vignetteRadius = 0.8f;
    cbData.vignetteSoftness = 0.5f;
    cbData.vignetteColor = Vec3(0.f, 0.f, 0.f);

    return shader->Bind_CBufferData(cbData);
}

HRESULT BattleTransition_ScreenFx::Bind_ShaderResources(const Shared<ShaderCom>& shader)
{
    HRESULT hr = GAME->Bind_RT_ShaderResource(shader, "g_ScreenMaskTexture", RT::ScreenCapture);

    if (_smudgeMaskSRV != nullptr)
        shader->Bind_SRV("g_SmudgeTexture", _smudgeMaskSRV);

    return hr;
}

void BattleTransition_ScreenFx::RoseFlare()
{
    EffectInstancePool* effectPool = CLIENT->Get_EffectInstancePool();
    if (effectPool != nullptr)
    {
        EffectInstancePool::AcquireDesc acquireDesc{};
        acquireDesc.effectName = L"Effects\\Assets\\Screen\\RoseFlare.effect.json";
        acquireDesc.layerTag = L"Layer_UI_Effect";

        acquireDesc.worldPosition = Vec3(0.f, 0.f, 0.f);
        effectPool->Acquire(acquireDesc, _roseFlareHandle);
    }
}

void BattleTransition_ScreenFx::Release_RoseFlare()
{
    if (_roseFlareHandle.effectObject == nullptr)
        return;

    EffectInstancePool* effectPool = CLIENT->Get_EffectInstancePool();
    if (effectPool != nullptr)
        effectPool->Release(_roseFlareHandle);

    _roseFlareHandle = {};
}

Shared<BattleTransition_ScreenFx> BattleTransition_ScreenFx::Create(BattleTransitionDesc* desc)
{
    auto instance = make_shared<BattleTransition_ScreenFx>();
    if (FAILED(instance->Initialize(desc)))
        return nullptr;
    return instance;
}

NS_END
