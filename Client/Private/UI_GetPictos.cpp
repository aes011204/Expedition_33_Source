#include "pch.h"
#include "UI_GetPictos.h"

#include "Client_Constants.h"
#include "ClientInstance.h"
#include "GameDataTable.h"
#include "GameInstance.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kGetItemAnimationName[] = L"Get_Item";
    constexpr wchar_t kPictosGetItemJsonDir[] = L"UI/UI_Json/CommonSystem/Get_Item/Pictos/";

    wstring Resolve_GetItemJsonPath(const string& getItemUi)
    {
        if (getItemUi.empty())
            return {};

        return wstring(kPictosGetItemJsonDir) + String::ToWString(getItemUi) + L".json";
    }

    wstring Resolve_GetItemRootName(const string& pictosId)
    {
        if (pictosId.empty())
            return {};

        return L"Panel_Get_Pictos_" + String::ToWString(pictosId);
    }

    wstring Resolve_GetItemPrototypeTagPrefix(const string& pictosId)
    {
        if (pictosId.empty())
            return {};

        return L"GetPictos" + String::ToWString(pictosId);
    }
}

HRESULT UI_GetPictos::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately();

    return S_OK;
}

void UI_GetPictos::Update(float timeDelta)
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

void UI_GetPictos::Open_ForPictosIds(const vector<string>& pictosIds)
{
    for (const string& rawPictosId : pictosIds)
    {
        const string pictosId = Normalize_PictosId(rawPictosId);
        if (pictosId.empty())
            continue;

        if (Is_PictosQueuedOrOpen(pictosId))
            continue;

        _queuedPictosIds.emplace_back(pictosId);
    }

    if (!Is_Open())
        Open_NextPictos();
}

void UI_GetPictos::Close()
{
    Finish_CurrentAndOpenNext();
}

void UI_GetPictos::Close_Immediately()
{
    _queuedPictosIds.clear();
    _activePictosId.clear();
    _elapsed = 0.f;

    Close_AllRoots();

    // 열린 상태에서 정지될 경우 다음 월드 입력을 막지 않도록 입력 차단은 반드시 복구한다.
    GAME->Set_PlayerInputBlocked(false);
}

HRESULT UI_GetPictos::Load_UI(uint32 levelIndex)
{
    UI_KeyframeAnimationAsset getItemAnimation{};
    CHECK_TRUE(
        UI_RuntimeLoader::Load_PublicAnimationAsset(Constants::UI_Anim::UI_GetItemAnim_Path, getItemAnimation),
        E_FAIL);

    _animationDuration = Get_KeyframeAnimationDurationSec(getItemAnimation);

    for (const GameDataTable::PictosDesc& pictosDesc : CLIENT->Get_PictosDescs())
    {
        if (pictosDesc.id.empty() || pictosDesc.getItemUi.empty())
            continue;

        umap<wstring, Shared<Engine::UI_Base>> loadedUIByName{};
        const wstring jsonPath = Resolve_GetItemJsonPath(pictosDesc.getItemUi);
        const wstring rootName = Resolve_GetItemRootName(pictosDesc.id);
        const wstring prototypeTagPrefix = Resolve_GetItemPrototypeTagPrefix(pictosDesc.id);

        UI_RuntimeLoader::UI_LoadDesc desc{};
        desc.levelIndex = levelIndex;
        desc.jsonPath = jsonPath;
        desc.prototypeTagPrefix = prototypeTagPrefix;
        desc.outUIByName = &loadedUIByName;

        CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

        Shared<Engine::UI_Base> root = Find_UI(loadedUIByName, rootName);
        CHECK_NULL(root, E_FAIL);

        root->Add_UIAnimationAsset(getItemAnimation);
        _pictosRoots.emplace(pictosDesc.id, root);
        _uiByName.insert(loadedUIByName.begin(), loadedUIByName.end());
    }

    if (!_pictosRoots.empty())
        _root = _pictosRoots.begin()->second;

    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_GetPictos::Bind_UI()
{
    CHECK_TRUE(!_pictosRoots.empty(), E_FAIL);
    return S_OK;
}

void UI_GetPictos::Open_NextPictos()
{
    while (!_queuedPictosIds.empty())
    {
        _activePictosId = _queuedPictosIds.front();
        _queuedPictosIds.erase(_queuedPictosIds.begin());

        _root = Resolve_Root(_activePictosId);
        if (_root == nullptr)
            continue;

        _elapsed = 0.f;

        Close_AllRoots();
        UI_RuntimePanelBase::Open();
        _root->Play_UIAnimation(kGetItemAnimationName, true);

        // Get Item 계열은 클릭 UI가 아니라 연출 UI라서 마우스/UIOnly 전환 없이 월드 조작만 막는다.
        GAME->Set_PlayerInputBlocked(true);
        return;
    }

    _activePictosId.clear();
}

void UI_GetPictos::Finish_CurrentAndOpenNext()
{
    Close_AllRoots();
    _activePictosId.clear();
    _elapsed = 0.f;

    // 현재 연출이 끝났으면 다음 보상 UI가 열릴 때까지 월드 입력을 복구한다.
    GAME->Set_PlayerInputBlocked(false);

    Open_NextPictos();
}

void UI_GetPictos::Close_AllRoots()
{
    for (auto& [pictosId, root] : _pictosRoots)
    {
        UNREFERENCED_PARAMETER(pictosId);

        if (root)
        {
            root->Stop_UIAnimation();
            root->Close();
        }
    }

    _isOpen = false;
}

Shared<Engine::UI_Base> UI_GetPictos::Resolve_Root(const string& pictosId) const
{
    const auto iter = _pictosRoots.find(pictosId);
    if (iter == _pictosRoots.end())
        return nullptr;

    return iter->second;
}

bool UI_GetPictos::Is_PictosQueuedOrOpen(const string& pictosId) const
{
    if (_activePictosId == pictosId)
        return true;

    return find(_queuedPictosIds.begin(), _queuedPictosIds.end(), pictosId) !=
        _queuedPictosIds.end();
}

string UI_GetPictos::Normalize_PictosId(const string& pictosId) const
{
    if (pictosId.empty())
        return {};

    if (_pictosRoots.find(pictosId) != _pictosRoots.end())
        return pictosId;

    return {};
}

Unique<UI_GetPictos> UI_GetPictos::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_GetPictos>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_GetPictos");
        instance->Free();
        return nullptr;
    }

    return instance;
}
