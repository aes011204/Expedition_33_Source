#include "ChangeWeapon_Panel.h"
#include "Client_Constants.h"
#include "ClientInstance.h"
#include "GameInstance.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"

HRESULT ChangeWeapon_Panel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();

    return S_OK;
}

void ChangeWeapon_Panel::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!_isOpen)
        return;
}

void ChangeWeapon_Panel::Open()
{
    _isOpen = true;

    if (_root)
        _root->Open();

    Refresh_WeaponUI();

    GAME->Unlock_Mouse();
    GAME->Set_GameplayPaused(true);
}

void ChangeWeapon_Panel::Close()
{
    _isOpen = false;

    if (_root)
        _root->Close();

    GAME->Set_GameplayPaused(false);
    GAME->Lock_Mouse();
}

void ChangeWeapon_Panel::Toggle()
{
    if (_isOpen)
        Close();
    else
        Open();
}

HRESULT ChangeWeapon_Panel::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_ChangeWeapon_Path;
    desc.prototypeTagPrefix = L"ChangeWeapon";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_ChangeWeapon");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT ChangeWeapon_Panel::Bind_UI()
{
    _tooltipText.weaponNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_WeaponName");
    _tooltipText.levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentWeaponLevel");
    _tooltipText.powerText = Find_TypedUI<UI_Text>(_uiByName, L"Text_WeaponTooltip_PowerValue");

    CHECK_NULL(_tooltipText.weaponNameText, E_FAIL);
    CHECK_NULL(_tooltipText.levelText, E_FAIL);
    CHECK_NULL(_tooltipText.powerText, E_FAIL);

    _weaponSelectTexts[0].weaponNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Lune_Baguette");
    _weaponSelectTexts[0].levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Weapon_LevelValue");

    _weaponSelectTexts[1].weaponNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Lune_Snowim");
    _weaponSelectTexts[1].levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Weapon_LevelValue_001");

    _weaponSelectTexts[2].weaponNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Lune_Esquim");
    _weaponSelectTexts[2].levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Weapon_LevelValue_002");

    for (const auto& weaponSelectText : _weaponSelectTexts)
    {
        CHECK_NULL(weaponSelectText.weaponNameText, E_FAIL);
        CHECK_NULL(weaponSelectText.levelText, E_FAIL);
    }

    return S_OK;
}

void ChangeWeapon_Panel::Refresh_WeaponUI()
{
    static const string defaultCharacterId = "Lune";

    const vector<GameDataTable::WeaponDesc>* weapons = CLIENT->Get_WeaponDescs(defaultCharacterId);
    if (weapons == nullptr || weapons->empty())
    {
        LOG_WARN("ChangeWeaponPanel weapon data not found: {}", defaultCharacterId);
        return;
    }

    if (_selectedWeaponIndex >= weapons->size())
        _selectedWeaponIndex = 0;

    Refresh_Tooltip((*weapons)[_selectedWeaponIndex]);
    Refresh_WeaponSelect(*weapons);
}

void ChangeWeapon_Panel::Refresh_Tooltip(const GameDataTable::WeaponDesc& weapon)
{
    Apply_WeaponSummary(_tooltipText, weapon);
}

void ChangeWeapon_Panel::Refresh_WeaponSelect(const vector<GameDataTable::WeaponDesc>& weapons)
{
    for (size_t i = 0; i < WeaponSlotCount && i < weapons.size(); ++i)
        Apply_WeaponSummary(_weaponSelectTexts[i], weapons[i]);
}

Shared<UI_Base> ChangeWeapon_Panel::Find_UI(
    const umap<wstring, Shared<UI_Base>>& uiByName,
    const wstring& name) const
{
    const auto iter = uiByName.find(name);
    if (iter == uiByName.end() || iter->second == nullptr)
        return nullptr;

    return iter->second;
}

Unique<ChangeWeapon_Panel> ChangeWeapon_Panel::Create(uint32 levelIndex)
{
    auto instance = make_unique<ChangeWeapon_Panel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : ChangeWeapon_Panel");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void ChangeWeapon_Panel::Free()
{
    if (_root)
        _root->Close();

    _tooltipText = {};
    _weaponSelectTexts = {};

    _root.reset();
    _uiByName.clear();

    Base::Free();
}
