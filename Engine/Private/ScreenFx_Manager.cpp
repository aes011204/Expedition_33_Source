#include "pch.h"
#include "ScreenFx_Manager.h"

#include "ShaderCom.h"
#include "VIBuffer_Rect.h"

ScreenFx_Manager::~ScreenFx_Manager()
{
    Clear_ScreenEffect();
}

void ScreenFx_Manager::Play_ScreenEffect(const Shared<ScreenFx_Instance>& effect)
{
    if (effect != nullptr)
        _activeEffects.push_back(effect);
}

void ScreenFx_Manager::Clear_ScreenEffect()
{
    _activeEffects.clear();
}

void ScreenFx_Manager::Update(float timeDelta)
{
    if (!_activeFx) return;

    memset(&_parameters, 0, sizeof(ScreenFxCB));

    _parameters.vignetteColor = Vec3::Zero;
    _parameters.vignetteIntensity = 0.f;

    for (auto it = _activeEffects.begin(); it != _activeEffects.end(); )
    {
        if ((*it)->Update_ScreenFx(timeDelta))
        {
            it = _activeEffects.erase(it);
        }
        else
        {
            (*it)->Apply_To(_parameters);
            ++it;
        }
    }
}

HRESULT ScreenFx_Manager::Render_ScreenFx(ShaderCom* shader, VIBuffer_Rect* rtBuffer)
{
    if (!shader || !rtBuffer) return E_FAIL;

    bool needsBloom = false;
    bool needsRadialBlur = false;
    bool needsChromaticAberration = false;
    bool needsVignette = false;

    // 활성화된 효과들을 순회하며 필요한 패스(Pass)들을 판별
    for (const auto& effect : _activeEffects)
    {
        switch (effect->Get_FxType())
        {
        case ScreenFx_Instance::ScreenFx_Type::Bloom : 
            needsBloom = true; break;
        case ScreenFx_Instance::ScreenFx_Type::RadialBlur :
            needsRadialBlur = true; break;
        case ScreenFx_Instance::ScreenFx_Type::ChromaticAberration:
            needsChromaticAberration = true; break;
        case ScreenFx_Instance::ScreenFx_Type::Vignette:
            needsVignette = true; break;
        case ScreenFx_Instance::ScreenFx_Type::ToneMapping:
            break;
        case ScreenFx_Instance::ScreenFx_Type::ColorGrading:
            break;
        case ScreenFx_Instance::ScreenFx_Type::Ghosting:
            break;
        case ScreenFx_Instance::ScreenFx_Type::Transition:
            break;
        case ScreenFx_Instance::ScreenFx_Type::Distortion:
            break;
        }
    }

    if (_activeFx)
    {
        if (FAILED(Bind_ScreenFxCBuffer(shader)))
            return E_FAIL;

        // 2. 만약 bNeedsBloom 이 true라면 기존 타겟 재활용(SRV 바인딩) - (나중에 HLSL 작성 시 필요)
        // GAME->Bind_RT_ShaderResource(shader, "g_BloomTexture", RT::BloomHalfA);

        uint32 passIndex = 0; // Default: Vignette (가정)

        if (needsRadialBlur && needsChromaticAberration)
        {
            passIndex = 3; // RadialBlurAndCA
        }
        else if (needsRadialBlur)
        {
            passIndex = 1; // RadialBlur
        }
        else if (needsChromaticAberration)
        {
            passIndex = 2; // ChromaticAberration
        }
        else if (needsBloom)
        {
            passIndex = 4; // Glow
        }

        if (FAILED(shader->Begin(passIndex)))
            return E_FAIL;

        if (FAILED(rtBuffer->Render()))
            return E_FAIL;
    }

    return S_OK;
}

HRESULT ScreenFx_Manager::Bind_ScreenFxCBuffer(ShaderCom* shader)
{
    if (shader == nullptr) return E_FAIL;
    return shader->Bind_CBufferData(_parameters);
}

Unique<ScreenFx_Manager> ScreenFx_Manager::Create()
{
    return make_unique<ScreenFx_Manager>();
}
