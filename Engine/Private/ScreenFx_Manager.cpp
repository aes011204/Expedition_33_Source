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

    for (auto it = _activeEffects.begin(); it != _activeEffects.end(); )
    {
        (*it)->Update_ScreenFx(timeDelta);

        if ((*it)->Check_EndCondition())
            it = _activeEffects.erase(it);
        else
            ++it;
    }
}

HRESULT ScreenFx_Manager::Render_ScreenFx(const Shared<ShaderCom>& shader, const Shared<VIBuffer_Rect>& rtBuffer)
{
    if (!shader || !rtBuffer) return E_FAIL;
    if (!_activeFx || _activeEffects.empty()) return S_OK;

    if (FAILED(GAME->Copy_CurrentRTV_To(RT::ScreenFxColorA)))
        return E_FAIL;
    bool readFromA = true;

    for (size_t i = 0; i < _activeEffects.size(); ++i)
    {
        auto& effect = _activeEffects[i];
        bool isLast = (i == _activeEffects.size() - 1);

        if (isLast)
        {
            GAME->Bind_BackBuffer();
        }
        else
        {
            const wchar_t* writeMRT = readFromA ? MRT::ScreenFxB : MRT::ScreenFxA;
            if (FAILED(GAME->Begin_MRT(writeMRT, false, true)))
                return E_FAIL;
        }

        const wchar_t* readRT = readFromA ? RT::ScreenFxColorA : RT::ScreenFxColorB;
        if (FAILED(GAME->Bind_RT_ShaderResource(shader, "g_ScreenFxTexture", readRT)))
            return E_FAIL;

        if (FAILED(effect->Bind_FxConstantBuffer(shader.get())))
            return E_FAIL;

        uint32 passIndex = effect->Get_PassIndex();
        if (FAILED(shader->Begin(passIndex)))
            return E_FAIL;

        if (FAILED(rtBuffer->Render()))
            return E_FAIL;

        if (!isLast)
        {
            if (FAILED(GAME->End_MRT()))
                return E_FAIL;
        }
        readFromA = !readFromA; // 타겟 스왑(Ping-Pong)
    }


    return S_OK;
}

Unique<ScreenFx_Manager> ScreenFx_Manager::Create()
{
    return make_unique<ScreenFx_Manager>();
}
