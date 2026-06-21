#include "pch.h"
#include "UI_GetClothes.h"

#include "Client_Constants.h"
#include "GameInstance.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

namespace
{
    constexpr char kBaguettePresentationId[] = "Baguette";
    constexpr char kPotatoBagPresentationId[] = "PotatoBag";
    constexpr wchar_t kBaguetteIntroAnimationName[] = L"Get_Clothes_Baguette_Intro";
    constexpr wchar_t kBaguetteOutroAnimationName[] = L"Get_Clothes_Baguette_Outro";
    constexpr wchar_t kPotatoBagIntroAnimationName[] = L"Get_Clothes_PotatoBag_Intro";
    constexpr wchar_t kPotatoBagOutroAnimationName[] = L"Get_Clothes_PotatoBag_Outro";
    constexpr float kGetClothesAutoOutroDelaySec = 1.f;
    constexpr float kGetClothesOutroDurationSec = 1.f; // Get_Clothes_*_Outro.uianim: 0~60f / 60fps

    string Normalize_ClothesPresentationId(const string& presetId)
    {
        if (presetId == "Baguette")
            return kBaguettePresentationId;

        if (presetId == "Potato" || presetId == "PotatoBag")
            return kPotatoBagPresentationId;

        return {};
    }
}

HRESULT UI_GetClothes::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately();

    return S_OK;
}

void UI_GetClothes::Update(float timeDelta)
{
    if (!Is_Open())
        return;

    if (_isClosingWithOutro)
    {
        _closeElapsed += max(0.f, timeDelta);

        if (_closeElapsed >= _outroDuration)
            Finish_CloseAndOpenNext();

        return;
    }

    if (GAME->KeyDown(KEY_TYPE::F) || GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        Close();
        return;
    }

    _openElapsed += max(0.f, timeDelta);
    if (_openElapsed >= kGetClothesAutoOutroDelaySec)
        Close();
}

void UI_GetClothes::Open_ForPresetIds(const vector<string>& presetIds)
{
    for (const string& presetId : presetIds)
    {
        const string presentationId = Normalize_ClothesPresentationId(presetId);
        if (presentationId.empty())
            continue;

        if (Is_PresetQueuedOrOpen(presentationId))
            continue;

        _queuedPresentationIds.emplace_back(presentationId);
    }

    if (!Is_Open())
        Open_NextPreset();
}

void UI_GetClothes::Close()
{
    if (!Is_Open())
    {
        Open_NextPreset();
        return;
    }

    if (_isClosingWithOutro)
        return;

    _isClosingWithOutro = true;
    _openElapsed = 0.f;
    _closeElapsed = 0.f;
    _outroDuration = kGetClothesOutroDurationSec;

    Play_TargetAnimation(Resolve_OutroAnimationName(_activePresentationId));

    if (_outroDuration <= 0.f)
        Finish_CloseAndOpenNext();
}

void UI_GetClothes::Close_Immediately()
{
    _isClosingWithOutro = false;
    _openElapsed = 0.f;
    _closeElapsed = 0.f;

    _queuedPresentationIds.clear();
    _activePresentationId.clear();

    Stop_TargetAnimations();
    Close_AllRoots();

    // Try_ShowPendingGetClothes가 기존 입력 차단 상태에서는 열지 않으므로, 여기서 우리가 건 차단만 복구한다.
    GAME->Set_PlayerInputBlocked(false);
}

HRESULT UI_GetClothes::Load_UI(uint32 levelIndex)
{
    {
        umap<wstring, Shared<Engine::UI_Base>> loadedUIByName{};

        UI_RuntimeLoader::UI_LoadDesc desc{};
        desc.levelIndex = levelIndex;
        desc.jsonPath = Constants::UI::UI_Get_Clothes_Baguette_Path;
        desc.prototypeTagPrefix = L"GetClothesBaguette";
        desc.outUIByName = &loadedUIByName;
        desc.animationPaths =
        {
            Constants::UI_Anim::UI_GetClothesBaguetteIntroAnim_Path,
            Constants::UI_Anim::UI_GetClothesBaguetteOutroAnim_Path,
        };

        CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

        _baguetteRoot = UI_Utils::Find_UI(loadedUIByName, L"Panel_Get_Clothes_Baguette");
        CHECK_NULL(_baguetteRoot, E_FAIL);

        _uiByName.insert(loadedUIByName.begin(), loadedUIByName.end());
    }

    {
        umap<wstring, Shared<Engine::UI_Base>> loadedUIByName{};

        UI_RuntimeLoader::UI_LoadDesc desc{};
        desc.levelIndex = levelIndex;
        desc.jsonPath = Constants::UI::UI_Get_Clothes_PotatoBag_Path;
        desc.prototypeTagPrefix = L"GetClothesPotatoBag";
        desc.outUIByName = &loadedUIByName;
        desc.animationPaths =
        {
            Constants::UI_Anim::UI_GetClothesPotatoBagIntroAnim_Path,
            Constants::UI_Anim::UI_GetClothesPotatoBagOutroAnim_Path,
        };

        CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

        _potatoBagRoot = UI_Utils::Find_UI(loadedUIByName, L"Panel_Get_Clothes_PotatoBag");
        CHECK_NULL(_potatoBagRoot, E_FAIL);

        _uiByName.insert(loadedUIByName.begin(), loadedUIByName.end());
    }

    _root = _baguetteRoot;

    return S_OK;
}

HRESULT UI_GetClothes::Bind_UI()
{
    CHECK_NULL(_baguetteRoot, E_FAIL);
    CHECK_NULL(_potatoBagRoot, E_FAIL);

    return S_OK;
}

void UI_GetClothes::Open_NextPreset()
{
    while (!_queuedPresentationIds.empty())
    {
        _activePresentationId = _queuedPresentationIds.front();
        _queuedPresentationIds.erase(_queuedPresentationIds.begin());

        _root = Resolve_Root(_activePresentationId);
        if (_root == nullptr)
            continue;

        _isClosingWithOutro = false;
        _openElapsed = 0.f;
        _closeElapsed = 0.f;

        Close_AllRoots();
        UI_RuntimePanelBase::Open();
        Play_TargetAnimation(Resolve_IntroAnimationName(_activePresentationId));

        // Get Clothes는 클릭 UI가 아니라 연출 UI라서 마우스/UIOnly 전환 없이 월드 조작만 막는다.
        GAME->Set_PlayerInputBlocked(true);
        return;
    }

    _activePresentationId.clear();
}

void UI_GetClothes::Finish_CloseAndOpenNext()
{
    _isClosingWithOutro = false;
    _openElapsed = 0.f;
    _closeElapsed = 0.f;

    Stop_TargetAnimations();
    Close_AllRoots();
    _activePresentationId.clear();

    // Get Clothes는 클릭 UI가 아니라 연출 UI라서 마우스/UIOnly 전환 없이 월드 조작만 막는다.
    GAME->Set_PlayerInputBlocked(false);

    Open_NextPreset();
}

void UI_GetClothes::Close_AllRoots()
{
    if (_baguetteRoot)
        _baguetteRoot->Close();

    if (_potatoBagRoot)
        _potatoBagRoot->Close();

    _isOpen = false;
}

Shared<Engine::UI_Base> UI_GetClothes::Resolve_Root(const string& presentationId) const
{
    if (presentationId == kBaguettePresentationId)
        return _baguetteRoot;

    if (presentationId == kPotatoBagPresentationId)
        return _potatoBagRoot;

    return nullptr;
}

bool UI_GetClothes::Is_PresetQueuedOrOpen(const string& presentationId) const
{
    if (_activePresentationId == presentationId)
        return true;

    return std::find(_queuedPresentationIds.begin(), _queuedPresentationIds.end(), presentationId) !=
        _queuedPresentationIds.end();
}

const wchar_t* UI_GetClothes::Resolve_IntroAnimationName(const string& presentationId) const
{
    if (presentationId == kBaguettePresentationId)
        return kBaguetteIntroAnimationName;

    if (presentationId == kPotatoBagPresentationId)
        return kPotatoBagIntroAnimationName;

    return L"";
}

const wchar_t* UI_GetClothes::Resolve_OutroAnimationName(const string& presentationId) const
{
    if (presentationId == kBaguettePresentationId)
        return kBaguetteOutroAnimationName;

    if (presentationId == kPotatoBagPresentationId)
        return kPotatoBagOutroAnimationName;

    return L"";
}

void UI_GetClothes::Play_TargetAnimation(const wchar_t* animationName)
{
    if (animationName == nullptr || animationName[0] == L'\0')
        return;

    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Play_UIAnimation(animationName, true);
    }
}

void UI_GetClothes::Stop_TargetAnimations()
{
    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Stop_UIAnimation();
    }
}

Unique<UI_GetClothes> UI_GetClothes::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_GetClothes>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_GetClothes");
        instance->Free();
        return nullptr;
    }

    return instance;
}


