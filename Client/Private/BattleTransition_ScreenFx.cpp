#include "pch.h"
#include "BattleTransition_ScreenFx.h"

#include "CA_ScreenFx.h"
#include "ClientScreenFxManager.h"
#include "GameInstance.h"
#include "Shader_CBuffer_Define.h"

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

    float time = std::clamp(_elapsedTime / max(0.001f, _duration), 0.0f, 1.0f);
    float easeOut = 1.0f - powf(1.0f - time, 3.0f);

    _currentRadius = _originalDesc.maxRadius * easeOut;
}

void BattleTransition_ScreenFx::Evaluate_At(float elapsedTime)
{
    // 리플랙션 뷰(에디터 타임라인)에서 0프레임으로 돌아갈 때 현재 화면을 캡처
    // 런타임에는 Evaluate_At이 아닌 Update_ScreenFx가 불리므로 영향이 없습니다.
    if (elapsedTime <= 0.001f)
    {
        GAME->Copy_CurrentRTV_To(RT::BackgroundMask);
    }

    _elapsedTime = elapsedTime;

    float time = std::clamp(_elapsedTime / max(0.001f, _duration), 0.0f, 1.0f);
    float easeOut = 1.0f - powf(1.0f - time, 3.0f);

    _currentRadius = _originalDesc.maxRadius * easeOut;
}

HRESULT BattleTransition_ScreenFx::Bind_FxConstantBuffer(ShaderCom* shader)
{
    Engine::ScreenFxCB cbData{};

    cbData.wipeRadius = _currentRadius;
    cbData.edgeSoftness = _originalDesc.edgeSoftness;
    cbData.aspectRatio = (float)GAME->Get_ViewportWidth() / (float)GAME->Get_ViewportHeight();
    return shader->Bind_CBufferData(cbData);
}

HRESULT BattleTransition_ScreenFx::Bind_ShaderResources(const Shared<ShaderCom>& shader)
{
    // 캡처해둔 기존 렌더 타겟(BackgroundMask)을 재활용하여 바인딩
    return GAME->Bind_RT_ShaderResource(shader, "g_ScreenMaskTexture", RT::BackgroundMask);
}

Shared<BattleTransition_ScreenFx> BattleTransition_ScreenFx::Create(BattleTransitionDesc* desc)
{
    auto instance = make_shared<BattleTransition_ScreenFx>();
    if (FAILED(instance->Initialize(desc)))
        return nullptr;
    return instance;
}

NS_END
