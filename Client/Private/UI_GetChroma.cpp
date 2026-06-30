#include "pch.h"
#include "UI_GetChroma.h"

#include "Client_Constants.h"
#include "GameInstance.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kGetItemAnimationName[] = L"Get_Item";
    constexpr wchar_t kChromaRootName[] = L"Panel_Get_Chroma";
    constexpr wchar_t kChromaValueTextName[] = L"Text_Get_Chroma_Value";
    constexpr wchar_t kChromaPrototypeTagPrefix[] = L"GetChroma";
}

HRESULT UI_GetChroma::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately();

    return S_OK;
}

void UI_GetChroma::Update(float timeDelta)
{
    if (!Is_Open())
        return;

    if (GAME->KeyDown(KEY_TYPE::F) || GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        Finish_CurrentAndOpenNext();
        return;
    }

    _elapsed += max(0.f, timeDelta);
    if (_elapsed >= _animationDuration)
        Finish_CurrentAndOpenNext();
}

void UI_GetChroma::Open_ForChromaAmounts(const vector<uint32>& chromaAmounts)
{
    for (uint32 chromaAmount : chromaAmounts)
    {
        if (chromaAmount == 0)
            continue;

        _queuedChromaAmounts.emplace_back(chromaAmount);
    }

    if (!Is_Open())
        Open_NextChroma();
}

void UI_GetChroma::Close()
{
    Finish_CurrentAndOpenNext();
}

void UI_GetChroma::Close_Immediately()
{
    _queuedChromaAmounts.clear();
    _activeChromaAmount = 0;
    _elapsed = 0.f;

    Close_Root();

    // Forced close must restore world input even if the reward presentation was still open.
    GAME->Set_PlayerInputBlocked(false);
}

HRESULT UI_GetChroma::Load_UI(uint32 levelIndex)
{
    UI_KeyframeAnimationAsset getItemAnimation{};
    CHECK_TRUE(
        UI_RuntimeLoader::Load_PublicAnimationAsset(Constants::UI_Anim::UI_GetItemAnim_Path, getItemAnimation),
        E_FAIL);

    _animationDuration = Get_KeyframeAnimationDurationSec(getItemAnimation);

    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Get_Chroma_Path;
    desc.prototypeTagPrefix = kChromaPrototypeTagPrefix;
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, kChromaRootName);
    CHECK_NULL(_root, E_FAIL);

    _root->Add_UIAnimationAsset(getItemAnimation);

    return S_OK;
}

HRESULT UI_GetChroma::Bind_UI()
{
    _chromaValueText = Find_TypedUI<Engine::UI_Text>(_uiByName, kChromaValueTextName, "GetChroma");
    CHECK_NULL(_chromaValueText, E_FAIL);

    return S_OK;
}

void UI_GetChroma::Open_NextChroma()
{
    while (!_queuedChromaAmounts.empty())
    {
        _activeChromaAmount = _queuedChromaAmounts.front();
        _queuedChromaAmounts.erase(_queuedChromaAmounts.begin());

        if (_activeChromaAmount == 0)
            continue;

        _elapsed = 0.f;

        Close_Root();
        _chromaValueText->Set_Text(to_wstring(_activeChromaAmount));
        UI_RuntimePanelBase::Open();
        _root->Play_UIAnimation(kGetItemAnimationName, true);

        // Get Item presentations are non-click UI, so only block world movement.
        GAME->Set_PlayerInputBlocked(true);
        return;
    }

    _activeChromaAmount = 0;
}

void UI_GetChroma::Finish_CurrentAndOpenNext()
{
    Close_Root();
    _activeChromaAmount = 0;
    _elapsed = 0.f;

    // Restore world input between reward presentations.
    GAME->Set_PlayerInputBlocked(false);

    Open_NextChroma();
}

void UI_GetChroma::Close_Root()
{
    if (_root)
    {
        _root->Stop_UIAnimation();
        _root->Close();
    }

    _isOpen = false;
}

Unique<UI_GetChroma> UI_GetChroma::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_GetChroma>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_GetChroma");
        instance->Free();
        return nullptr;
    }

    return instance;
}
