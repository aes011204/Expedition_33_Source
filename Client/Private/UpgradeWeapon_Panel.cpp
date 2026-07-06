#include "UpgradeWeapon_Panel.h"

#include "Client_Constants.h"
#include "ClientInstance.h"
#include "GameInstance.h"
#include "PlayerInventory.h"
#include "PlayerStatCom.h"
#include "Player_WorldBase.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr const wchar_t* kRoundWeaponBgPath = L"../Bin/Resources/Textures/UI/Inventory/Weapons/Background/T_UI_RoundWeaponBackground.png";
    constexpr const wchar_t* kLongWeaponBgPath = L"../Bin/Resources/Textures/UI/Inventory/Weapons/Background/T_UI_LongWeaponBackground.png";
    constexpr const char* kChromaCatalystMaterialId = "ChromaCatalyst";

    constexpr array<UpgradeWeapon_Panel::WeaponSlotDef, UpgradeWeapon_Panel::WeaponSlotCount> kWeaponSlots =
    {
        UpgradeWeapon_Panel::WeaponSlotDef{ HeroId::Lune,    "Lune",    "Baguette", L"Lune_Baguette",    L"Lune_Baguette",    L"Lune_Baguette" },
        UpgradeWeapon_Panel::WeaponSlotDef{ HeroId::Lune,    "Lune",    "Snowim",   L"Lune_Snowim",      L"Snowim",           L"Lune_Snowim" },
        UpgradeWeapon_Panel::WeaponSlotDef{ HeroId::Lune,    "Lune",    "Colim",    L"Lune_Colim",       L"Colim",            L"Lune_Colim" },
        UpgradeWeapon_Panel::WeaponSlotDef{ HeroId::Gustave, "Gustave", "Baguette", L"Gustave_Baguette", L"Gustave_Baguette", L"Gustave_Baguette" },
        UpgradeWeapon_Panel::WeaponSlotDef{ HeroId::Gustave, "Gustave", "Lanceram", L"Gustave_Lanceram", L"Lanceram",         L"Gustave_Lanceram" },
        UpgradeWeapon_Panel::WeaponSlotDef{ HeroId::Gustave, "Gustave", "Tireso",   L"Gustave_Tireso",   L"Tireso",           L"Gustave_Tireso" },
        UpgradeWeapon_Panel::WeaponSlotDef{ HeroId::Maelle,  "Maelle",  "Medalum",  L"Maelle_Medalum",   L"Maellum",          L"Maelle_Maellum" },
        UpgradeWeapon_Panel::WeaponSlotDef{ HeroId::Maelle,  "Maelle",  "Baguette", L"Maelle_Baguette",  L"Maelle_Baguette",  L"Maelle_Baguette" },
        UpgradeWeapon_Panel::WeaponSlotDef{ HeroId::Maelle,  "Maelle",  "Plenum",   L"Maelle_Plenum",    L"Plenum",           L"Maelle_Plenum" },
    };

    void Set_UIMapOpenState(const umap<wstring, Shared<UI_Base>>& uiByName, bool isOpen)
    {
        for (const auto& [uiName, ui] : uiByName)
        {
            UNREFERENCED_PARAMETER(uiName);

            if (ui == nullptr)
                continue;

            if (isOpen)
                ui->Open();
            else
                ui->Close();
        }
    }

    wstring Resolve_WeaponBackgroundTexturePath(const string& weaponId)
    {
        if (weaponId == "Colim" || weaponId == "Esquim")
            return kRoundWeaponBgPath;

        if (weaponId == "Baguette" ||
            weaponId == "Lanceram" ||
            weaponId == "Candy" ||
            weaponId == "Medalum" ||
            weaponId == "Sekarum" ||
            weaponId == "Snowim" ||
            weaponId == "Tireso" ||
            weaponId == "Plenum")
            return kLongWeaponBgPath;

        return L"";
    }

    wstring Make_FragmentPath(const wchar_t* uiSuffix)
    {
        return L"UI/UI_Json/CommonSystem/UpgradeWeapon/UI_Upgrade_Weapon_Fragment_" +
            wstring(uiSuffix ? uiSuffix : L"") + L".json";
    }

    wstring Make_UpgradeSuccessAnimationPath(const wchar_t* animationToken)
    {
        return L"UI/UI_Animations/Common/Upgrade_Weapon/Upgrade_" +
            wstring(animationToken ? animationToken : L"");
    }

    bool Starts_With(const wstring& value, const wstring& prefix)
    {
        return value.rfind(prefix, 0) == 0;
    }

    bool Ends_With(const wstring& value, const wstring& suffix)
    {
        return value.size() >= suffix.size() &&
            value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    template<typename T>
    Shared<T> Find_TypedUIByPrefixSuffix(
        const umap<wstring, Shared<UI_Base>>& uiByName,
        const wstring& prefix,
        const wstring& suffix,
        const char* ownerTag)
    {
        for (const auto& [name, ui] : uiByName)
        {
            if (!Starts_With(name, prefix) || !Ends_With(name, suffix))
                continue;

            Shared<T> typedUI = dynamic_pointer_cast<T>(ui);
            if (typedUI == nullptr)
                LOG_WARN("{} ui type mismatch: {}", ownerTag, String::ToString(name));

            return typedUI;
        }

        return nullptr;
    }

    void Bind_GradeTexts(
        const umap<wstring, Shared<UI_Base>>& uiByName,
        WeaponSummaryText& detail,
        const wstring& prefix)
    {
        detail.vitalityGradeText = Find_TypedUI<UI_Text>(uiByName, prefix + L"Vitality_Value", "UpgradeWeapon");
        detail.mightGradeText = Find_TypedUI<UI_Text>(uiByName, prefix + L"Might_Value", "UpgradeWeapon");
        detail.agilityGradeText = Find_TypedUI<UI_Text>(uiByName, prefix + L"Agility_Value", "UpgradeWeapon");
        detail.defenseGradeText = Find_TypedUI<UI_Text>(uiByName, prefix + L"Defense_Value", "UpgradeWeapon");
        detail.luckGradeText = Find_TypedUI<UI_Text>(uiByName, prefix + L"Luck_Value", "UpgradeWeapon");
    }
}

HRESULT UpgradeWeapon_Panel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately(false);

    return S_OK;
}

void UpgradeWeapon_Panel::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_Open())
        return;

    Handle_WeaponInput();
    Handle_UpgradeInput();
}

void UpgradeWeapon_Panel::Open()
{
    if (_root == nullptr)
        return;

    if (Is_Open())
        return;

    UI_RuntimePanelBase::Open();
    Set_UIMapOpenState(_uiByName, true);

    _selectedWeaponIndex.reset();
    Refresh_WeaponRows();
    Close_AllDetailFragments();
    Apply_BaseDim(false);

    GAME->Play_SoundCue(Constants::SoundCue::UI_Global_GenericEnter);
    GAME->Unlock_Mouse();
    GAME->Set_GameplayPaused(true);
}

void UpgradeWeapon_Panel::Close()
{
    Close_Immediately(true);
}

void UpgradeWeapon_Panel::Close_Immediately(bool restoreWorldControl)
{
    _selectedWeaponIndex.reset();
    UI_RuntimePanelBase::Close();
    Set_UIMapOpenState(_uiByName, false);
    Close_AllDetailFragments();
    Apply_BaseDim(false);

    if (restoreWorldControl)
    {
        GAME->Set_GameplayPaused(false);
        GAME->Lock_Mouse();
    }
}

void UpgradeWeapon_Panel::Close_ForMenuTransition()
{
    Close_Immediately(false);
}

bool UpgradeWeapon_Panel::Consume_BackInput()
{
    if (!Is_Open() || !_selectedWeaponIndex.has_value())
        return false;

    const size_t previousWeaponIndex = *_selectedWeaponIndex;
    _selectedWeaponIndex.reset();

    Refresh_HoverDetails(previousWeaponIndex);
    Apply_BaseDim(false);

    return true;
}

void UpgradeWeapon_Panel::Append_RuntimeUIEntries(
    vector<RuntimeUIEntry>& outEntries,
    const string& category,
    const string& rootName) const
{
    if (_root == nullptr)
        return;

    RuntimeUIEntry rootEntry{};
    rootEntry.category = category;
    rootEntry.name = rootName;
    rootEntry.ui = _root;
    rootEntry.editorTarget = const_cast<UpgradeWeapon_Panel*>(this);
    outEntries.push_back(rootEntry);

    Append_RuntimeUIMapEntries(outEntries, category, _uiByName, _root, const_cast<UpgradeWeapon_Panel*>(this));
}

HRESULT UpgradeWeapon_Panel::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_UpgradeWeapon_Path;
    desc.prototypeTagPrefix = L"UpgradeWeapon";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_Upgrade_Weapon");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UpgradeWeapon_Panel::Load_UIFragment(size_t weaponIndex)
{
    if (weaponIndex >= WeaponSlotCount)
        return E_FAIL;

    WeaponFragmentUI& fragment = _weaponFragments[weaponIndex];
    if (fragment.loaded)
        return S_OK;

    const WeaponSlotDef& slot = kWeaponSlots[weaponIndex];

    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = _levelIndex;
    desc.jsonPath = Make_FragmentPath(slot.uiSuffix);
    desc.animationPaths.push_back(Make_UpgradeSuccessAnimationPath(slot.upgradeAnimationToken));
    desc.prototypeTagPrefix = L"UpgradeWeaponFragment_" + wstring(slot.uiSuffix);

    umap<wstring, Shared<UI_Base>> loadedUIByName{};
    desc.outUIByName = &loadedUIByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    Set_UIMapOpenState(loadedUIByName, Is_Open());

    CHECK_FAILED(Bind_FragmentUI(weaponIndex, loadedUIByName), E_FAIL);

    for (auto& [name, ui] : loadedUIByName)
        _uiByName[name] = ui;

    fragment.loaded = true;
    Apply_FragmentDisplayMode(weaponIndex, FragmentDisplayMode::Hidden);

    return S_OK;
}

HRESULT UpgradeWeapon_Panel::Bind_UI()
{
    _weaponSelectRoot = Find_UI(_uiByName, L"Panel_WeaponSelect");
    _backgroundRoot = Find_UI(_uiByName, L"Panel_Upgrade_Weapon_BG");

    CHECK_NULL(_weaponSelectRoot, E_FAIL);
    CHECK_NULL(_backgroundRoot, E_FAIL);

    for (size_t i = 0; i < WeaponSlotCount; ++i)
    {
        const wstring suffix = kWeaponSlots[i].uiSuffix;
        WeaponRowUI& row = _weaponRows[i];

        row.button = Find_TypedUI<UI_Button>(_uiByName, L"Button_" + suffix, "UpgradeWeapon");
        row.icon = Find_TypedUI<UI_Image>(_uiByName, L"Image_" + suffix, "UpgradeWeapon");
        row.background = Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_" + suffix + L"_BG", "UpgradeWeapon");
        row.weaponNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_" + suffix, "UpgradeWeapon");
        row.levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Weapon_LevelValue_" + suffix, "UpgradeWeapon");

        CHECK_NULL(row.button, E_FAIL);
        CHECK_NULL(row.icon, E_FAIL);
        CHECK_NULL(row.background, E_FAIL);
        CHECK_NULL(row.weaponNameText, E_FAIL);
        CHECK_NULL(row.levelText, E_FAIL);
    }

    return S_OK;
}

HRESULT UpgradeWeapon_Panel::Bind_FragmentUI(size_t weaponIndex, const umap<wstring, Shared<UI_Base>>& fragmentUIByName)
{
    if (weaponIndex >= WeaponSlotCount)
        return E_FAIL;

    const WeaponSlotDef& slot = kWeaponSlots[weaponIndex];
    const wstring token = slot.fragmentToken;
    WeaponFragmentUI& fragment = _weaponFragments[weaponIndex];

    fragment.tooltipRoot = Find_UI(fragmentUIByName, L"Panel_" + token + L"_Tooltip");
    fragment.confirmRoot = Find_UI(fragmentUIByName, L"Panel_" + token + L"_Upgrade");
    fragment.hoverLevel5Lock = Find_UI(fragmentUIByName, L"Panel_" + token + L"_Level_5_Tooltip_Lock");
    fragment.hoverLevel8Lock = Find_UI(fragmentUIByName, L"Panel_" + token + L"_Level_8_Tooltip_Lock");
    fragment.confirmBeforeLevel5Lock = Find_UI(fragmentUIByName, L"Panel_Upgrade_Before_" + token + L"_Level_5_Tooltip_Lock");
    fragment.confirmBeforeLevel8Lock = Find_UI(fragmentUIByName, L"Panel_Upgrade_Before_" + token + L"_Level_8_Tooltip_Lock");
    fragment.confirmAfterLevel5Lock = Find_UI(fragmentUIByName, L"Panel_Upgrade_After_" + token + L"_Level_5_Tooltip_Lock");
    fragment.confirmAfterLevel8Lock = Find_UI(fragmentUIByName, L"Panel_Upgrade_After_" + token + L"_Level_8_Tooltip_Lock");

    fragment.hoverDetail.weaponNameText = Find_TypedUI<UI_Text>(fragmentUIByName, L"Text_" + token + L"_Tooltip", "UpgradeWeapon");
    fragment.hoverDetail.levelText = Find_TypedUIByPrefixSuffix<UI_Text>(
        fragmentUIByName, L"Text_" + token + L"_Tooltip_Current_", L"_Level_Value", "UpgradeWeapon");
    fragment.hoverDetail.powerText = Find_TypedUI<UI_Text>(fragmentUIByName, L"Text_" + token + L"_Tooltip_Power_Value", "UpgradeWeapon");
    Bind_GradeTexts(fragmentUIByName, fragment.hoverDetail, L"Text_" + token + L"_Tooltip_");
    fragment.hoverThumbnail = Find_TypedUI<UI_Image>(fragmentUIByName, L"Image_" + token + L"_Thumnail_Tooltip", "UpgradeWeapon");
    fragment.hoverThumbnailBackground = Find_TypedUI<UI_Image>(fragmentUIByName, L"Image_" + token + L"_Thumnail_BG_Tooltip", "UpgradeWeapon");

    fragment.confirmBeforeDetail.weaponNameText = Find_TypedUI<UI_Text>(fragmentUIByName, L"Text_Upgrade_Before_" + token + L"_Tooltip", "UpgradeWeapon");
    fragment.confirmBeforeDetail.levelText = Find_TypedUIByPrefixSuffix<UI_Text>(
        fragmentUIByName, L"Text_Upgrade_Before_" + token + L"_Tooltip_Current_", L"_Level_Value", "UpgradeWeapon");
    fragment.confirmBeforeDetail.powerText = Find_TypedUI<UI_Text>(fragmentUIByName, L"Text_Upgrade_Before_" + token + L"_Tooltip_Power_Value", "UpgradeWeapon");
    Bind_GradeTexts(fragmentUIByName, fragment.confirmBeforeDetail, L"Text_Upgrade_Before_" + token + L"_Tooltip_");
    fragment.confirmBeforeThumbnail = Find_TypedUI<UI_Image>(fragmentUIByName, L"Image_Upgrade_Before_" + token + L"_Thumnail_Tooltip", "UpgradeWeapon");
    fragment.confirmBeforeThumbnailBackground = Find_TypedUI<UI_Image>(fragmentUIByName, L"Image_Upgrade_Before_" + token + L"_Thumnail_BG_Tooltip", "UpgradeWeapon");

    fragment.confirmAfterDetail.weaponNameText = Find_TypedUI<UI_Text>(fragmentUIByName, L"Text_Upgrade_After_" + token + L"_Tooltip", "UpgradeWeapon");
    fragment.confirmAfterDetail.levelText = Find_TypedUIByPrefixSuffix<UI_Text>(
        fragmentUIByName, L"Text_Upgrade_After_" + token + L"_Tooltip_Current_", L"_Level_Value", "UpgradeWeapon");
    fragment.confirmAfterDetail.powerText = Find_TypedUI<UI_Text>(fragmentUIByName, L"Text_Upgrade_After_" + token + L"_Tooltip_Power_Value", "UpgradeWeapon");
    Bind_GradeTexts(fragmentUIByName, fragment.confirmAfterDetail, L"Text_Upgrade_After_" + token + L"_Tooltip_");
    fragment.confirmAfterThumbnail = Find_TypedUI<UI_Image>(fragmentUIByName, L"Image_Upgrade_After_" + token + L"_Thumnail_Tooltip", "UpgradeWeapon");
    fragment.confirmAfterThumbnailBackground = Find_TypedUI<UI_Image>(fragmentUIByName, L"Image_Upgrade_After_" + token + L"_Thumnail_BG_Tooltip", "UpgradeWeapon");

    fragment.requiredRoot = Find_UI(fragmentUIByName, L"Panel_Upgrade_Required");
    fragment.currentCatalystText = Find_TypedUI<UI_Text>(fragmentUIByName, L"Text_Current_Chroma_Catalyst", "UpgradeWeapon");
    fragment.requiredCatalystText = Find_TypedUI<UI_Text>(fragmentUIByName, L"Text_Required_Chroma_Catalyst", "UpgradeWeapon");
    fragment.upgradeButton = Find_TypedUI<UI_Button>(fragmentUIByName, L"Button_Weapon_Upgrade", "UpgradeWeapon");
    fragment.upgradeButtonText = Find_TypedUI<UI_Text>(fragmentUIByName, L"Text_Weapon_Upgrade", "UpgradeWeapon");

    CHECK_NULL(fragment.tooltipRoot, E_FAIL);
    CHECK_NULL(fragment.confirmRoot, E_FAIL);
    CHECK_NULL(fragment.hoverDetail.weaponNameText, E_FAIL);
    CHECK_NULL(fragment.hoverDetail.levelText, E_FAIL);
    CHECK_NULL(fragment.hoverDetail.powerText, E_FAIL);
    CHECK_NULL(fragment.hoverThumbnail, E_FAIL);
    CHECK_NULL(fragment.hoverThumbnailBackground, E_FAIL);
    CHECK_NULL(fragment.confirmBeforeDetail.weaponNameText, E_FAIL);
    CHECK_NULL(fragment.confirmBeforeDetail.levelText, E_FAIL);
    CHECK_NULL(fragment.confirmBeforeDetail.powerText, E_FAIL);
    CHECK_NULL(fragment.confirmBeforeThumbnail, E_FAIL);
    CHECK_NULL(fragment.confirmBeforeThumbnailBackground, E_FAIL);
    CHECK_NULL(fragment.confirmAfterDetail.weaponNameText, E_FAIL);
    CHECK_NULL(fragment.confirmAfterDetail.levelText, E_FAIL);
    CHECK_NULL(fragment.confirmAfterDetail.powerText, E_FAIL);
    CHECK_NULL(fragment.confirmAfterThumbnail, E_FAIL);
    CHECK_NULL(fragment.confirmAfterThumbnailBackground, E_FAIL);
    CHECK_NULL(fragment.requiredRoot, E_FAIL);
    CHECK_NULL(fragment.currentCatalystText, E_FAIL);
    CHECK_NULL(fragment.requiredCatalystText, E_FAIL);
    CHECK_NULL(fragment.upgradeButton, E_FAIL);
    CHECK_NULL(fragment.upgradeButtonText, E_FAIL);

    return S_OK;
}

void UpgradeWeapon_Panel::Handle_WeaponInput()
{
    if (_selectedWeaponIndex.has_value())
        return;

    optional<size_t> hoveredIndex{};

    for (size_t i = 0; i < WeaponSlotCount; ++i)
    {
        const WeaponRowUI& row = _weaponRows[i];
        if (row.button == nullptr || !row.button->Is_Open())
            continue;

        if (row.button->Is_Hovered())
            hoveredIndex = i;

        if (row.button->Was_Clicked())
        {
            if (FAILED(Ensure_FragmentLoaded(i)))
                return;

            _selectedWeaponIndex = i;
            Refresh_SelectedDetails(true);
            GAME->Play_SoundCue(Constants::SoundCue::UI_Attribute_Navigate);
            return;
        }
    }

    if (hoveredIndex.has_value())
        Refresh_HoverDetails(*hoveredIndex);
    else
    {
        Close_AllDetailFragments();
        Apply_BaseDim(false);
    }
}

void UpgradeWeapon_Panel::Handle_UpgradeInput()
{
    if (!_selectedWeaponIndex.has_value())
        return;

    const size_t weaponIndex = *_selectedWeaponIndex;
    WeaponFragmentUI& fragment = _weaponFragments[weaponIndex];
    const bool requestedByButton = fragment.upgradeButton != nullptr && fragment.upgradeButton->Was_Clicked();
    const bool requestedByKey = GAME->KeyDown(KEY_TYPE::F);
    if (!requestedByButton && !requestedByKey)
        return;

    Try_UpgradeSelectedWeapon();
}

bool UpgradeWeapon_Panel::Try_UpgradeSelectedWeapon()
{
    if (!_selectedWeaponIndex.has_value())
        return false;

    const size_t weaponIndex = *_selectedWeaponIndex;
    WeaponFragmentUI& fragment = _weaponFragments[weaponIndex];
    const WeaponSlotDef& slot = kWeaponSlots[weaponIndex];
    const GameDataTable::WeaponDesc* weapon = CLIENT->Get_WeaponDesc(slot.characterId, slot.weaponId);
    const auto inventory = CLIENT->Get_PlayerInventory();
    if (weapon == nullptr || inventory == nullptr)
        return false;

    const uint32 currentLevel = inventory->Get_WeaponEnhanceLevel(slot.characterId, slot.weaponId);
    const GameDataTable::WeaponUpgradeDesc* nextUpgrade =
        CLIENT->Get_NextWeaponUpgradeDesc(slot.characterId, slot.weaponId, currentLevel);

    if (nextUpgrade == nullptr)
        return false;

    if (!inventory->Spend_Material(kChromaCatalystMaterialId, nextUpgrade->requiredChromaCatalyst))
    {
        if (fragment.upgradeButtonText)
            fragment.upgradeButtonText->Set_Text(L"재료 부족");
        return false;
    }

    if (!inventory->Set_WeaponEnhanceLevel(slot.characterId, slot.weaponId, nextUpgrade->level))
        return false;

    Apply_EquippedWeaponPower(slot.characterId, slot.weaponId, nextUpgrade->power);
    Refresh_WeaponRows();
    Refresh_SelectedDetails(true);
    Play_UpgradeSuccessAnimation(weaponIndex);
    GAME->Play_SoundCue(Constants::SoundCue::UI_Attribute_UpgradeAccept);
    return true;
}

void UpgradeWeapon_Panel::Play_UpgradeSuccessAnimation(size_t weaponIndex)
{
    if (weaponIndex >= WeaponSlotCount)
        return;

    const wchar_t* animationToken = kWeaponSlots[weaponIndex].upgradeAnimationToken;
    if (animationToken == nullptr || animationToken[0] == L'\0')
        return;

    const wstring animationName = L"Upgrade_" + wstring(animationToken);
    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Play_UIAnimation(animationName, true);
    }
}

void UpgradeWeapon_Panel::Refresh_WeaponRows()
{
    const auto inventory = CLIENT->Get_PlayerInventory();

    for (size_t i = 0; i < WeaponSlotCount; ++i)
    {
        const WeaponSlotDef& slot = kWeaponSlots[i];
        WeaponRowUI& row = _weaponRows[i];
        const GameDataTable::WeaponDesc* weapon = CLIENT->Get_WeaponDesc(slot.characterId, slot.weaponId);

        if (weapon == nullptr || inventory == nullptr || !inventory->Is_WeaponUnlocked(slot.characterId, slot.weaponId))
        {
            if (row.button)
                row.button->Close();
            continue;
        }

        row.button->Open();

        if (row.weaponNameText)
            row.weaponNameText->Set_Text(weapon->weaponName);

        if (row.levelText)
            row.levelText->Set_Text(to_wstring(inventory->Get_WeaponEnhanceLevel(slot.characterId, slot.weaponId)));

        Apply_Thumbnail(row.icon, *weapon);
        Apply_ThumbnailBackground(row.background, weapon->weaponId);
    }
}

void UpgradeWeapon_Panel::Refresh_SelectedDetails(bool showConfirm)
{
    if (!_selectedWeaponIndex.has_value())
        return;

    const size_t weaponIndex = *_selectedWeaponIndex;
    if (FAILED(Ensure_FragmentLoaded(weaponIndex)))
        return;

    const WeaponSlotDef& slot = kWeaponSlots[weaponIndex];
    const GameDataTable::WeaponDesc* weapon = CLIENT->Get_WeaponDesc(slot.characterId, slot.weaponId);
    if (weapon == nullptr)
        return;

    WeaponFragmentUI& fragment = _weaponFragments[weaponIndex];
    const uint32 currentLevel = CLIENT->Resolve_WeaponEnhanceLevel(slot.characterId, slot.weaponId);
    const uint32 currentPower = CLIENT->Resolve_WeaponPower(slot.characterId, slot.weaponId);
    const GameDataTable::WeaponUpgradeDesc* currentUpgrade =
        CLIENT->Get_WeaponUpgradeDesc(slot.characterId, slot.weaponId, currentLevel);
    const GameDataTable::WeaponUpgradeDesc* nextUpgrade =
        CLIENT->Get_NextWeaponUpgradeDesc(slot.characterId, slot.weaponId, currentLevel);

    Apply_DetailUI(fragment.confirmBeforeDetail, *weapon, currentUpgrade, currentLevel, currentPower);
    Apply_Thumbnail(fragment.confirmBeforeThumbnail, *weapon);
    Apply_ThumbnailBackground(fragment.confirmBeforeThumbnailBackground, weapon->weaponId);

    if (nextUpgrade != nullptr)
        Apply_DetailUI(fragment.confirmAfterDetail, *weapon, nextUpgrade, nextUpgrade->level, nextUpgrade->power);
    else
        Apply_DetailUI(fragment.confirmAfterDetail, *weapon, currentUpgrade, currentLevel, currentPower);
    const uint32 afterLevel = nextUpgrade ? nextUpgrade->level : currentLevel;

    Apply_Thumbnail(fragment.confirmAfterThumbnail, *weapon);
    Apply_ThumbnailBackground(fragment.confirmAfterThumbnailBackground, weapon->weaponId);

    Close_AllDetailFragments();
    Apply_MilestoneLocks(weaponIndex, currentLevel, currentLevel, afterLevel);
    Refresh_Required(weaponIndex, *weapon, currentLevel, showConfirm);
    Apply_FragmentDisplayMode(
        weaponIndex,
        showConfirm ? FragmentDisplayMode::SelectedUpgrade : FragmentDisplayMode::HoverPreview);
    Apply_BaseDim(showConfirm);
}

void UpgradeWeapon_Panel::Refresh_HoverDetails(size_t weaponIndex)
{
    if (FAILED(Ensure_FragmentLoaded(weaponIndex)))
        return;

    const WeaponSlotDef& slot = kWeaponSlots[weaponIndex];
    const GameDataTable::WeaponDesc* weapon = CLIENT->Get_WeaponDesc(slot.characterId, slot.weaponId);
    if (weapon == nullptr)
        return;

    WeaponFragmentUI& fragment = _weaponFragments[weaponIndex];
    const uint32 currentLevel = CLIENT->Resolve_WeaponEnhanceLevel(slot.characterId, slot.weaponId);
    const uint32 currentPower = CLIENT->Resolve_WeaponPower(slot.characterId, slot.weaponId);
    const GameDataTable::WeaponUpgradeDesc* currentUpgrade =
        CLIENT->Get_WeaponUpgradeDesc(slot.characterId, slot.weaponId, currentLevel);
    Apply_DetailUI(fragment.hoverDetail, *weapon, currentUpgrade, currentLevel, currentPower);
    Apply_Thumbnail(fragment.hoverThumbnail, *weapon);
    Apply_ThumbnailBackground(fragment.hoverThumbnailBackground, weapon->weaponId);

    Close_AllDetailFragments();
    Apply_MilestoneLocks(weaponIndex, currentLevel, currentLevel, currentLevel);
    Refresh_Required(weaponIndex, *weapon, currentLevel, false);
    Apply_FragmentDisplayMode(weaponIndex, FragmentDisplayMode::HoverPreview);
    Apply_BaseDim(false);
}

void UpgradeWeapon_Panel::Refresh_Required(
    size_t weaponIndex,
    const GameDataTable::WeaponDesc& weapon,
    uint32 currentLevel,
    bool allowUpgradeInput)
{
    if (weaponIndex >= WeaponSlotCount)
        return;

    WeaponFragmentUI& fragment = _weaponFragments[weaponIndex];
    const auto inventory = CLIENT->Get_PlayerInventory();
    const GameDataTable::WeaponUpgradeDesc* nextUpgrade =
        CLIENT->Get_NextWeaponUpgradeDesc(weapon.characterId, weapon.weaponId, currentLevel);

    const uint32 currentCatalyst = inventory ? inventory->Get_MaterialCount(kChromaCatalystMaterialId) : 0;
    const uint32 requiredCatalyst = nextUpgrade ? nextUpgrade->requiredChromaCatalyst : 0;

    if (fragment.currentCatalystText)
        fragment.currentCatalystText->Set_Text(to_wstring(currentCatalyst));

    if (fragment.requiredCatalystText)
        fragment.requiredCatalystText->Set_Text(nextUpgrade ? to_wstring(requiredCatalyst) : L"MAX");

    const bool hasNextUpgrade = nextUpgrade != nullptr;
    const bool hasEnoughCatalyst = currentCatalyst >= requiredCatalyst;
    const bool canUpgrade = allowUpgradeInput && hasNextUpgrade && hasEnoughCatalyst;
    if (fragment.upgradeButton)
        fragment.upgradeButton->Set_Disabled(!canUpgrade);

    if (fragment.upgradeButtonText)
    {
        if (!hasNextUpgrade)
            fragment.upgradeButtonText->Set_Text(L"최대 강화");
        else if (!hasEnoughCatalyst)
            fragment.upgradeButtonText->Set_Text(L"재료 부족");
        else
            fragment.upgradeButtonText->Set_Text(L"무기 업그레이드");
    }
}

void UpgradeWeapon_Panel::Apply_DetailUI(
    const WeaponSummaryText& detailUI,
    const GameDataTable::WeaponDesc& weapon,
    const GameDataTable::WeaponUpgradeDesc* upgrade,
    uint32 fallbackLevel,
    uint32 fallbackPower)
{
    if (upgrade)
    {
        Apply_WeaponUpgradeSummary(detailUI, weapon, *upgrade);
        return;
    }

    Apply_WeaponSummary(detailUI, weapon, fallbackLevel, fallbackPower);
}

void UpgradeWeapon_Panel::Apply_Thumbnail(const Shared<UI_Image>& image, const GameDataTable::WeaponDesc& weapon)
{
    if (image == nullptr || weapon.portraitTexturePath.empty())
        return;

    CHECK_FAILED(image->Set_TexturePath(weapon.portraitTexturePath));
}

void UpgradeWeapon_Panel::Apply_ThumbnailBackground(const Shared<UI_Image>& image, const string& weaponId)
{
    if (image == nullptr)
        return;

    const wstring backgroundTexturePath = Resolve_WeaponBackgroundTexturePath(weaponId);
    if (!backgroundTexturePath.empty())
        CHECK_FAILED(image->Set_TexturePath(backgroundTexturePath));
}

void UpgradeWeapon_Panel::Apply_EquippedWeaponPower(const string& characterId, const string& weaponId, uint32 power)
{
    const auto heroId = magic_enum::enum_cast<HeroId>(characterId);
    if (!heroId.has_value())
        return;

    const Shared<Player_WorldBase> hero = CLIENT->Find_Hero(*heroId);
    const Shared<PlayerStatCom> statCom = hero ? hero->Get_Component<PlayerStatCom>() : nullptr;
    if (statCom == nullptr)
        return;

    if (statCom->Get_PlayerRuntimeStat().CurEquippedWeaponId == weaponId)
        statCom->Set_EquippedWeapon(weaponId, power);
}

void UpgradeWeapon_Panel::Apply_MilestoneLocks(size_t weaponIndex, uint32 hoverLevel, uint32 beforeLevel, uint32 afterLevel)
{
    if (weaponIndex >= WeaponSlotCount)
        return;

    const auto applyLock = [](const Shared<UI_Base>& lockRoot, uint32 displayLevel, uint32 milestoneLevel)
        {
            if (lockRoot == nullptr)
                return;

            const bool shouldShow = displayLevel < milestoneLevel;
            lockRoot->Set_AlphaMultiplier(shouldShow ? 1.f : 0.f);

            if (shouldShow && !lockRoot->Is_Open())
                lockRoot->Open();
            else if (!shouldShow && lockRoot->Is_Open())
                lockRoot->Close();
        };

    WeaponFragmentUI& fragment = _weaponFragments[weaponIndex];
    applyLock(fragment.hoverLevel5Lock, hoverLevel, 5);
    applyLock(fragment.hoverLevel8Lock, hoverLevel, 8);
    applyLock(fragment.confirmBeforeLevel5Lock, beforeLevel, 5);
    applyLock(fragment.confirmBeforeLevel8Lock, beforeLevel, 8);
    applyLock(fragment.confirmAfterLevel5Lock, afterLevel, 5);
    applyLock(fragment.confirmAfterLevel8Lock, afterLevel, 8);
}

HRESULT UpgradeWeapon_Panel::Ensure_FragmentLoaded(size_t weaponIndex)
{
    if (weaponIndex >= WeaponSlotCount)
        return E_FAIL;

    if (_weaponFragments[weaponIndex].loaded)
        return S_OK;

    return Load_UIFragment(weaponIndex);
}

void UpgradeWeapon_Panel::Apply_FragmentDisplayMode(size_t weaponIndex, FragmentDisplayMode mode)
{
    if (weaponIndex >= WeaponSlotCount)
        return;

    WeaponFragmentUI& fragment = _weaponFragments[weaponIndex];
    const bool showAny = mode != FragmentDisplayMode::Hidden;
    const bool showTooltip = mode == FragmentDisplayMode::HoverPreview;
    const bool showUpgrade = mode == FragmentDisplayMode::SelectedUpgrade;

    Show_WeaponTooltip(weaponIndex, showAny);
    Show_Confirm(weaponIndex, showAny);
    Show_Required(weaponIndex, showAny);

    if (fragment.tooltipRoot)
        fragment.tooltipRoot->Set_AlphaMultiplier(showTooltip ? 1.f : 0.f);

    if (fragment.confirmRoot)
        fragment.confirmRoot->Set_AlphaMultiplier(showUpgrade ? 1.f : 0.f);

    if (fragment.requiredRoot)
        fragment.requiredRoot->Set_AlphaMultiplier(showAny ? 1.f : 0.f);

    if (fragment.upgradeButton && mode != FragmentDisplayMode::SelectedUpgrade)
        fragment.upgradeButton->Set_Disabled(true);
}

void UpgradeWeapon_Panel::Show_WeaponTooltip(size_t weaponIndex, bool isVisible)
{
    if (weaponIndex >= WeaponSlotCount)
        return;

    Shared<UI_Base> tooltipRoot = _weaponFragments[weaponIndex].tooltipRoot;
    if (tooltipRoot == nullptr)
        return;

    if (isVisible && !tooltipRoot->Is_Open())
        tooltipRoot->Open();
    else if (!isVisible && tooltipRoot->Is_Open())
        tooltipRoot->Close();
}

void UpgradeWeapon_Panel::Close_AllWeaponTooltips()
{
    for (WeaponFragmentUI& fragment : _weaponFragments)
    {
        if (fragment.tooltipRoot)
        {
            fragment.tooltipRoot->Set_AlphaMultiplier(0.f);
            fragment.tooltipRoot->Close();
        }
    }
}

void UpgradeWeapon_Panel::Show_Required(size_t weaponIndex, bool isVisible)
{
    if (weaponIndex >= WeaponSlotCount)
        return;

    Shared<UI_Base> requiredRoot = _weaponFragments[weaponIndex].requiredRoot;
    if (requiredRoot == nullptr)
        return;

    if (isVisible && !requiredRoot->Is_Open())
        requiredRoot->Open();
    else if (!isVisible && requiredRoot->Is_Open())
        requiredRoot->Close();
}

void UpgradeWeapon_Panel::Show_Confirm(size_t weaponIndex, bool isVisible)
{
    if (weaponIndex >= WeaponSlotCount)
        return;

    Shared<UI_Base> confirmRoot = _weaponFragments[weaponIndex].confirmRoot;
    if (confirmRoot == nullptr)
        return;

    if (isVisible && !confirmRoot->Is_Open())
        confirmRoot->Open();
    else if (!isVisible && confirmRoot->Is_Open())
        confirmRoot->Close();
}

void UpgradeWeapon_Panel::Close_AllConfirmFragments()
{
    for (WeaponFragmentUI& fragment : _weaponFragments)
    {
        if (fragment.confirmRoot)
        {
            fragment.confirmRoot->Set_AlphaMultiplier(0.f);
            fragment.confirmRoot->Close();
        }

        if (fragment.upgradeButton)
            fragment.upgradeButton->Set_Disabled(true);
    }
}

void UpgradeWeapon_Panel::Close_AllRequiredPanels()
{
    for (WeaponFragmentUI& fragment : _weaponFragments)
    {
        if (fragment.requiredRoot)
        {
            fragment.requiredRoot->Set_AlphaMultiplier(0.f);
            fragment.requiredRoot->Close();
        }
    }
}

void UpgradeWeapon_Panel::Close_AllDetailFragments()
{
    Close_AllWeaponTooltips();
    Close_AllConfirmFragments();
    Close_AllRequiredPanels();
}

void UpgradeWeapon_Panel::Apply_BaseDim(bool isDimmed)
{
    if (_weaponSelectRoot)
        _weaponSelectRoot->Set_AlphaMultiplier(isDimmed ? 0.38f : 1.f);

    if (_backgroundRoot)
        _backgroundRoot->Set_AlphaMultiplier(isDimmed ? 0.55f : 1.f);
}

Unique<UpgradeWeapon_Panel> UpgradeWeapon_Panel::Create(uint32 levelIndex)
{
    auto instance = make_unique<UpgradeWeapon_Panel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UpgradeWeapon_Panel");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UpgradeWeapon_Panel::Free()
{
    if (Is_Open())
        Close_Immediately(false);

    UI_RuntimeLoader::Release_Document(_uiByName);
    _root.reset();
    _weaponSelectRoot.reset();
    _backgroundRoot.reset();

    for (WeaponRowUI& row : _weaponRows)
    {
        row = {};
    }

    for (WeaponFragmentUI& fragment : _weaponFragments)
    {
        fragment = {};
    }

    Base::Free();
}
