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

    for (int32 i = 0; i < static_cast<int32>(_activeEffects.size()); )
    {
        _activeEffects[i]->Update_ScreenFx(timeDelta);

        if (_activeEffects[i]->Check_EndCondition())
        {
            _activeEffects.erase(_activeEffects.begin() + i);
        }
        else
        {
            ++i;
        }
    }
}

HRESULT ScreenFx_Manager::Render_ScreenFx(const Shared<ShaderCom>& shader, const Shared<VIBuffer_Rect>& rtBuffer)
{
    if (!shader || !rtBuffer) return E_FAIL;
    
    bool hasActiveEffects = !_activeEffects.empty();
#ifdef _DEBUG
    if (_previewEffect) hasActiveEffects = true;
#endif

    if (!_activeFx || !hasActiveEffects) return S_OK;

    if (FAILED(GAME->Copy_CurrentRTV_To(RT::ScreenFxColorA)))
        return E_FAIL;
    bool readFromA = true;

    for (size_t i = 0; i < _activeEffects.size(); ++i)
    {
        auto& effect = _activeEffects[i];
        bool isLast = (i == _activeEffects.size() - 1);

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
    if (_previewEffect)
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
