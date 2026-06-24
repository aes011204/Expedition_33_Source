#include "pch.h"
#include "ScreenFx_Manager.h"
#include "GameInstance.h"
#include "Renderer_Define.h"
#include "ShaderCom.h"
#include "VIBuffer_Rect.h"

ScreenFx_Manager::~ScreenFx_Manager()
{
    Clear_ScreenEffect();
}

void ScreenFx_Manager::Play_ScreenEffect(const Shared<ScreenFx_Instance>& effect)
{
    Play_ScreenEffect_WithHandle(effect);
}

ScreenFxHandle ScreenFx_Manager::Play_ScreenEffect_WithHandle(const Shared<ScreenFx_Instance>& effect)
{
    if (effect == nullptr)
        return {};

    if (!effect->Get_Tag().empty())
        _finishedEffectTags.erase(effect->Get_Tag());

    ScreenFxHandle handle{};
    handle.id = _nextEffectHandleId++;
    if (_nextEffectHandleId == 0)
        _nextEffectHandleId = 1;

    _activeEffects.push_back({ handle, effect });
    return handle;
}

void ScreenFx_Manager::Stop_ScreenEffect(ScreenFxHandle handle)
{
    if (!handle.Is_Valid())
        return;

    const auto iter = std::find_if(
        _activeEffects.begin(),
        _activeEffects.end(),
        [handle](const ActiveEffectEntry& entry)
        {
            return entry.handle.id == handle.id;
        });

    if (iter == _activeEffects.end())
        return;

    _activeEffects.erase(iter);
}

ScreenFxHandle ScreenFx_Manager::Play_PreviewScreenEffect_WithHandle(const Shared<ScreenFx_Instance>& effect)
{
    if (effect == nullptr)
        return {};

    ScreenFxHandle handle{};
    handle.id = _nextEffectHandleId++;
    if (_nextEffectHandleId == 0)
        _nextEffectHandleId = 1;

    _previewEffects.push_back({ handle, effect });
    return handle;
}

void ScreenFx_Manager::Evaluate_PreviewScreenEffect(ScreenFxHandle handle, float elapsedTime)
{
    if (!handle.Is_Valid())
        return;

    const auto iter = std::find_if(
        _previewEffects.begin(),
        _previewEffects.end(),
        [handle](const ActiveEffectEntry& entry)
        {
            return entry.handle.id == handle.id;
        });

    if (iter == _previewEffects.end() || iter->effect == nullptr)
        return;

    iter->effect->Evaluate_At(max(0.f, elapsedTime));
}

void ScreenFx_Manager::Stop_PreviewScreenEffect(ScreenFxHandle handle)
{
    if (!handle.Is_Valid())
        return;

    const auto iter = std::find_if(
        _previewEffects.begin(),
        _previewEffects.end(),
        [handle](const ActiveEffectEntry& entry)
        {
            return entry.handle.id == handle.id;
        });

    if (iter == _previewEffects.end())
        return;

    _previewEffects.erase(iter);
}

void ScreenFx_Manager::Clear_ScreenEffect()
{
    _activeEffects.clear();
}

bool ScreenFx_Manager::Is_ScreenEffectFinished(const wstring& tag) const
{
    if (tag.empty())
        return false;

    return _finishedEffectTags.contains(tag);
}

void ScreenFx_Manager::Update(float timeDelta)
{
    if (!_activeFx) return;

    auto updateEffects = [&](vector<ActiveEffectEntry>& effects, bool recordFinishedTags)
    {
        for (int32 i = 0; i < static_cast<int32>(effects.size()); )
        {
            Shared<ScreenFx_Instance>& effect = effects[i].effect;
            if (effect == nullptr)
            {
                effects.erase(effects.begin() + i);
                continue;
            }

            effect->Update_ScreenFx(timeDelta);

            if (effect->Check_EndCondition())
            {
                if (recordFinishedTags && !effect->Get_Tag().empty())
                    _finishedEffectTags.insert(effect->Get_Tag());

                effects.erase(effects.begin() + i);
            }
            else
            {
                ++i;
            }
        }
    };

    updateEffects(_activeEffects, true);
}

HRESULT ScreenFx_Manager::Render_ScreenFx(const Shared<ShaderCom>& shader, const Shared<VIBuffer_Rect>& rtBuffer)
{
    if (!shader || !rtBuffer) return E_FAIL;
    
    vector<ActiveEffectEntry>& effects = _renderPreviewEffectsOnly ? _previewEffects : _activeEffects;
    bool hasActiveEffects = !effects.empty();
#ifdef _DEBUG
    if (!_renderPreviewEffectsOnly && _previewEffect) hasActiveEffects = true;
#endif

    if (!_activeFx || !hasActiveEffects) return S_OK;

    if (FAILED(GAME->Copy_CurrentRTV_To(RT::ScreenFxColorA)))
        return E_FAIL;
    bool readFromA = true;

    for (size_t i = 0; i < effects.size(); ++i)
    {
        auto& effect = effects[i].effect;
        if (effect == nullptr)
            continue;

        bool isLast = (i == effects.size() - 1);

        if (isLast && _captureRequested)
        {
            const wchar_t* writeMRT = readFromA ? MRT::ScreenFxB : MRT::ScreenFxA;
            if (FAILED(GAME->Begin_MRT(writeMRT, false, true))) return E_FAIL;

            const wchar_t* readRT = readFromA ? RT::ScreenFxColorA : RT::ScreenFxColorB;
            if (FAILED(GAME->Bind_RT_ShaderResource(shader, "g_ScreenFxTexture", readRT))) return E_FAIL;
            if (FAILED(effect->Bind_FxConstantBuffer(shader.get()))) return E_FAIL;
            if (FAILED(effect->Bind_ShaderResources(shader))) return E_FAIL;
            if (FAILED(shader->Begin(effect->Get_PassIndex()))) return E_FAIL;
            if (FAILED(rtBuffer->Render())) return E_FAIL;
            if (FAILED(GAME->End_MRT())) return E_FAIL;

            const wchar_t* writtenRT = readFromA ? RT::ScreenFxColorB : RT::ScreenFxColorA;
            GAME->Copy_RTV(writtenRT, RT::ScreenCapture);
            _captureRequested = false;
        }
        else if (isLast)
        {
            // 일반적인 마지막 패스
            const wchar_t* readRT = readFromA ? RT::ScreenFxColorA : RT::ScreenFxColorB;
            if (FAILED(GAME->Bind_RT_ShaderResource(shader, "g_ScreenFxTexture", readRT)))
                return E_FAIL;
            if (FAILED(effect->Bind_FxConstantBuffer(shader.get())))
                return E_FAIL;
            if (FAILED(effect->Bind_ShaderResources(shader)))
                return E_FAIL;
            if (FAILED(shader->Begin(effect->Get_PassIndex())))
                return E_FAIL;
            if (FAILED(rtBuffer->Render()))
                return E_FAIL;
        }
        else
        {
            // 중간 패스
            const wchar_t* writeMRT = readFromA ? MRT::ScreenFxB : MRT::ScreenFxA;
            if (FAILED(GAME->Begin_MRT(writeMRT, false, true)))
                return E_FAIL;
            
            const wchar_t* readRT = readFromA ? RT::ScreenFxColorA : RT::ScreenFxColorB;
            if (FAILED(GAME->Bind_RT_ShaderResource(shader, "g_ScreenFxTexture", readRT)))
                return E_FAIL;
            if (FAILED(effect->Bind_FxConstantBuffer(shader.get())))
                return E_FAIL;
            if (FAILED(effect->Bind_ShaderResources(shader)))
                return E_FAIL;
            if (FAILED(shader->Begin(effect->Get_PassIndex())))
                return E_FAIL;
            if (FAILED(rtBuffer->Render()))
                return E_FAIL;
            if (FAILED(GAME->End_MRT()))
                return E_FAIL;
            
            readFromA = !readFromA; // 타겟 스왑(Ping-Pong)
        }
    }

#ifdef _DEBUG
    if (!_renderPreviewEffectsOnly && _previewEffect)
    {
        const wchar_t* readRT = readFromA ? RT::ScreenFxColorA : RT::ScreenFxColorB;
        GAME->Bind_RT_ShaderResource(shader, "g_ScreenFxTexture", readRT);
        _previewEffect->Bind_FxConstantBuffer(shader.get());
        _previewEffect->Bind_ShaderResources(shader);
        shader->Begin(_previewEffect->Get_PassIndex());
        rtBuffer->Render();
    }
#endif


    return S_OK;
}

Unique<ScreenFx_Manager> ScreenFx_Manager::Create()
{
    return make_unique<ScreenFx_Manager>();
}

void ScreenFx_Manager::Request_ScreenCapture(bool saveToFile)
{
    _captureRequested = true;
    _saveRequested = saveToFile;
}

HRESULT ScreenFx_Manager::Save_ScreenCapture(const wstring& filePath)
{
    return S_OK;
}
