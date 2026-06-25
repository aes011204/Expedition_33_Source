#include "pch.h"
#include "UI_GetWeapon.h"

#include "Client_Constants.h"
#include "GameInstance.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kGetItemAnimationName[] = L"Get_Item";
    constexpr char kGustaveBaguettePresentationId[] = "Gustave_Baguette";
    constexpr char kLuneBaguettePresentationId[] = "Lune_Baguette";
    constexpr char kMaelleBaguettePresentationId[] = "Maelle_Baguette";

    struct GetWeaponPresentationDesc
    {
        const char* id;
        const wchar_t* jsonPath;
        const wchar_t* rootName;
        const wchar_t* prototypeTagPrefix;
    };

    const array<GetWeaponPresentationDesc, 9> kWeaponPresentations =
    {
        GetWeaponPresentationDesc{ "Gustave_Baguette", Constants::UI::UI_Get_Weapon_Gustave_Baguette_Path, L"Panel_Get_Weapon_Gustave_Baguette", L"GetWeaponGustaveBaguette" },
        GetWeaponPresentationDesc{ "Gustave_Candy", Constants::UI::UI_Get_Weapon_Gustave_Candy_Path, L"Panel_Get_Weapon_Gustave_Candy", L"GetWeaponGustaveCandy" },
        GetWeaponPresentationDesc{ "Gustave_Lanceram", Constants::UI::UI_Get_Weapon_Gustave_Lanceram_Path, L"Panel_Get_Weapon_Gustave_Lanceram", L"GetWeaponGustaveLanceram" },
        GetWeaponPresentationDesc{ "Lune_Baguette", Constants::UI::UI_Get_Weapon_Lune_Baguette_Path, L"Panel_Get_Weapon_Lune_Baguette", L"GetWeaponLuneBaguette" },
        GetWeaponPresentationDesc{ "Lune_Colim", Constants::UI::UI_Get_Weapon_Lune_Colim_Path, L"Panel_Get_Weapon_Lune_Colim", L"GetWeaponLuneColim" },
        GetWeaponPresentationDesc{ "Lune_Snowim", Constants::UI::UI_Get_Weapon_Lune_Snowim_Path, L"Panel_Get_Weapon_Lune_Snowim", L"GetWeaponLuneSnowim" },
        GetWeaponPresentationDesc{ "Maelle_Baguette", Constants::UI::UI_Get_Weapon_Maelle_Baguette_Path, L"Panel_Get_Weapon_Maelle_Baguette", L"GetWeaponMaelleBaguette" },
        GetWeaponPresentationDesc{ "Maelle_Medalum", Constants::UI::UI_Get_Weapon_Maelle_Medalum_Path, L"Panel_Get_Weapon_Maelle_Medalum", L"GetWeaponMaelleMedalum" },
        GetWeaponPresentationDesc{ "Maelle_Sekarum", Constants::UI::UI_Get_Weapon_Maelle_Sekarum_Path, L"Panel_Get_Weapon_Maelle_Sekarum", L"GetWeaponMaelleSekarum" },
    };

    bool Contains_PresentationId(const vector<string>& presentationIds, const string& targetId)
    {
        return find(presentationIds.begin(), presentationIds.end(), targetId) != presentationIds.end();
    }

    void Erase_PresentationId(vector<string>& presentationIds, const string& targetId)
    {
        presentationIds.erase(
            remove(presentationIds.begin(), presentationIds.end(), targetId),
            presentationIds.end());
    }
}

HRESULT UI_GetWeapon::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately();

    return S_OK;
}

void UI_GetWeapon::Update(float timeDelta)
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

void UI_GetWeapon::Open_ForWeaponIds(const vector<string>& weaponPresentationIds)
{
    vector<string> pendingIds{};

    for (const string& weaponPresentationId : weaponPresentationIds)
    {
        const string presentationId = Normalize_WeaponPresentationId(weaponPresentationId);
        if (presentationId.empty())
            continue;

        if (Is_WeaponQueuedOrOpen(presentationId))
            continue;

        if (Contains_PresentationId(pendingIds, presentationId))
            continue;

        pendingIds.emplace_back(presentationId);
    }

    const bool hasBaguetteGroup =
        Contains_PresentationId(pendingIds, kGustaveBaguettePresentationId) &&
        Contains_PresentationId(pendingIds, kLuneBaguettePresentationId) &&
        Contains_PresentationId(pendingIds, kMaelleBaguettePresentationId);

    if (hasBaguetteGroup)
    {
        _queuedPresentationGroups.push_back({
            kGustaveBaguettePresentationId,
            kLuneBaguettePresentationId,
            kMaelleBaguettePresentationId,
        });

        Erase_PresentationId(pendingIds, kGustaveBaguettePresentationId);
        Erase_PresentationId(pendingIds, kLuneBaguettePresentationId);
        Erase_PresentationId(pendingIds, kMaelleBaguettePresentationId);
    }

    for (const string& presentationId : pendingIds)
    {
        _queuedPresentationGroups.push_back({ presentationId });
    }

    if (!Is_Open())
        Open_NextWeapon();
}

void UI_GetWeapon::Close()
{
    Finish_CurrentAndOpenNext();
}

void UI_GetWeapon::Close_Immediately()
{
    _queuedPresentationGroups.clear();
    _activePresentationIds.clear();
    _elapsed = 0.f;

    Close_AllRoots();

    // 아직 런타임 보상 소비와 연결하지 않은 연출 UI지만, 열린 상태에서 정지될 경우 입력 차단은 반드시 복구한다.
    GAME->Set_PlayerInputBlocked(false);
}

HRESULT UI_GetWeapon::Load_UI(uint32 levelIndex)
{
    UI_KeyframeAnimationAsset getItemAnimation{};
    CHECK_TRUE(
        UI_RuntimeLoader::Load_PublicAnimationAsset(Constants::UI_Anim::UI_GetItemAnim_Path, getItemAnimation),
        E_FAIL);

    _animationDuration = Get_KeyframeAnimationDurationSec(getItemAnimation);

    for (const GetWeaponPresentationDesc& presentation : kWeaponPresentations)
    {
        umap<wstring, Shared<Engine::UI_Base>> loadedUIByName{};

        UI_RuntimeLoader::UI_LoadDesc desc{};
        desc.levelIndex = levelIndex;
        desc.jsonPath = presentation.jsonPath;
        desc.prototypeTagPrefix = presentation.prototypeTagPrefix;
        desc.outUIByName = &loadedUIByName;

        CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

        Shared<Engine::UI_Base> root = Find_UI(loadedUIByName, presentation.rootName);
        CHECK_NULL(root, E_FAIL);

        root->Add_UIAnimationAsset(getItemAnimation);
        _weaponRoots.emplace(presentation.id, root);
        _uiByName.insert(loadedUIByName.begin(), loadedUIByName.end());
    }

    _root = Resolve_Root(kWeaponPresentations.front().id);
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_GetWeapon::Bind_UI()
{
    CHECK_TRUE(!_weaponRoots.empty(), E_FAIL);
    return S_OK;
}

void UI_GetWeapon::Open_NextWeapon()
{
    while (!_queuedPresentationGroups.empty())
    {
        _activePresentationIds = _queuedPresentationGroups.front();
        _queuedPresentationGroups.erase(_queuedPresentationGroups.begin());

        vector<Shared<Engine::UI_Base>> roots{};
        for (const string& presentationId : _activePresentationIds)
        {
            Shared<Engine::UI_Base> root = Resolve_Root(presentationId);
            if (root != nullptr)
                roots.emplace_back(root);
        }

        if (roots.empty())
            continue;

        _elapsed = 0.f;

        Close_AllRoots();
        _root = roots.front();
        UI_RuntimePanelBase::Open();
        for (const Shared<Engine::UI_Base>& root : roots)
        {
            if (root != _root)
                root->Open();

            for (const string& presentationId : _activePresentationIds)
            {
                if (Resolve_Root(presentationId) == root)
                {
                    Apply_ActiveGroupRootLayout(presentationId, root);
                    break;
                }
            }

            root->Play_UIAnimation(kGetItemAnimationName, true);
        }

        // Get Item 계열은 클릭 UI가 아니라 연출 UI라서 마우스/UIOnly 전환 없이 월드 조작만 막는다.
        GAME->Set_PlayerInputBlocked(true);
        return;
    }

    _activePresentationIds.clear();
}

void UI_GetWeapon::Finish_CurrentAndOpenNext()
{
    Close_AllRoots();
    _activePresentationIds.clear();
    _elapsed = 0.f;

    // 현재 연출이 끝났으면 다음 보상 UI가 열릴 때까지 월드 입력을 복구한다.
    GAME->Set_PlayerInputBlocked(false);

    Open_NextWeapon();
}

void UI_GetWeapon::Close_AllRoots()
{
    for (auto& [presentationId, root] : _weaponRoots)
    {
        UNREFERENCED_PARAMETER(presentationId);

        if (root)
        {
            root->Stop_UIAnimation();
            root->Close();
        }
    }

    _isOpen = false;
}

Shared<Engine::UI_Base> UI_GetWeapon::Resolve_Root(const string& presentationId) const
{
    const auto iter = _weaponRoots.find(presentationId);
    if (iter == _weaponRoots.end())
        return nullptr;

    return iter->second;
}

bool UI_GetWeapon::Is_WeaponQueuedOrOpen(const string& presentationId) const
{
    if (Contains_PresentationId(_activePresentationIds, presentationId))
        return true;

    for (const vector<string>& queuedGroup : _queuedPresentationGroups)
    {
        if (Contains_PresentationId(queuedGroup, presentationId))
            return true;
    }

    return false;
}

void UI_GetWeapon::Apply_ActiveGroupRootLayout(const string& presentationId, const Shared<Engine::UI_Base>& root)
{
    if (root == nullptr || root->Get_Transform() == nullptr)
        return;

    const bool isBaguetteGroup =
        Contains_PresentationId(_activePresentationIds, kGustaveBaguettePresentationId) &&
        Contains_PresentationId(_activePresentationIds, kLuneBaguettePresentationId) &&
        Contains_PresentationId(_activePresentationIds, kMaelleBaguettePresentationId);
    if (!isBaguetteGroup)
        return;

    Vec2 position = root->Get_Transform()->Get_Position();

    if (presentationId == kGustaveBaguettePresentationId)
        position.x -= 440.f;
    else if (presentationId == kMaelleBaguettePresentationId)
        position.x += 440.f;
    else
        return;

    root->Get_Transform()->Set_Position(position);
}

string UI_GetWeapon::Normalize_WeaponPresentationId(const string& presentationId) const
{
    if (presentationId.empty())
        return {};

    if (_weaponRoots.find(presentationId) != _weaponRoots.end())
        return presentationId;

    return {};
}

Unique<UI_GetWeapon> UI_GetWeapon::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_GetWeapon>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_GetWeapon");
        instance->Free();
        return nullptr;
    }

    return instance;
}
