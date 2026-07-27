#include "ChangeWeapon_Panel.h"

#include "Client_Constants.h"
#include "ClientInstance.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "LightingPipeline.h"
#include "PlayerInventory.h"
#include "Player_WorldBase.h"
#include "PlayerStatCom.h"
#include "PostProcessPipeline.h"
#include "PreviewPlayer.h"
#include "PreviewRimFeature.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UIPreviewManager.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    struct CurrentWeaponTooltipSlotDef
    {
        const char* characterId = "";
        const char* weaponId = "";
        const wchar_t* tooltipToken = L"";
    };

    constexpr array<CurrentWeaponTooltipSlotDef, 9> kCurrentWeaponTooltipSlots =
    {
        CurrentWeaponTooltipSlotDef{ "Gustave", "Lanceram", L"Lanceram" },
        CurrentWeaponTooltipSlotDef{ "Gustave", "Baguette", L"Gustave_Baguette" },
        CurrentWeaponTooltipSlotDef{ "Gustave", "Tireso",   L"Tireso" },
        CurrentWeaponTooltipSlotDef{ "Lune",    "Colim",    L"Colim" },
        CurrentWeaponTooltipSlotDef{ "Lune",    "Baguette", L"Lune_Baguette" },
        CurrentWeaponTooltipSlotDef{ "Lune",    "Snowim",   L"Snowim" },
        CurrentWeaponTooltipSlotDef{ "Maelle",  "Medalum",  L"Maellum" },
        CurrentWeaponTooltipSlotDef{ "Maelle",  "Baguette", L"Maelle_Baguette" },
        CurrentWeaponTooltipSlotDef{ "Maelle",  "Plenum",   L"Plenum" },
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

    void Close_ChangeWeaponBlackBackground(const umap<wstring, Shared<UI_Base>>& uiByName)
    {
        const auto iter = uiByName.find(L"Image_BlackBG");
        if (iter != uiByName.end() && iter->second)
            iter->second->Close();
    }

    vector<UIPreviewManager::PreviewSlotDesc> Build_ChangeWeaponPreviewLayout()
    {
        return
        {
            { HeroId::Lune,    Vec3(1.1f,  0.f, -0.1f), Vec3(0.f, 0.f, -0.1f) },
            { HeroId::Gustave, Vec3(0.0f,  0.f,  0.f), Vec3(0.f, 0.f,  0.f) },
            { HeroId::Maelle,  Vec3(-1.1f, 0.f, -0.1f), Vec3(0.f, 0.f, -0.1f) },
        };
    }

    void Show_ChangeWeaponPreview(HeroId heroId)
    {
        const auto previewLayout = Build_ChangeWeaponPreviewLayout();
        CLIENT->Apply_PreviewLayout(previewLayout);
        CLIENT->Show_Preview(heroId, PreviewShotPanel::ChangeWeapon);
    }

    void Apply_ChangeWeaponPreviewWeapon(HeroId heroId)
    {
        const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
        if (previewManager == nullptr)
            return;

        const auto& previewHeroes = previewManager->Get_PreviewHeroes();
        const int32 heroIndex = static_cast<int32>(heroId);
        if (heroIndex < 0 || heroIndex >= static_cast<int32>(previewHeroes.size()))
            return;

        const Shared<PreviewPlayer>& previewPlayer = previewHeroes[heroIndex].player;
        if (previewPlayer == nullptr)
            return;

        CHECK_FAILED(previewPlayer->Reload_ChangeWeaponPreviewOffsetsFromPrefab()); // 저장된 PP_* offset을 기존 preview 인스턴스에 다시 반영한다.
        CHECK_FAILED(previewPlayer->Apply_EquippedWeaponFromRuntimeData()); // ChangeWeapon 화면에서만 실제 장착 무기를 preview에 붙인다.
    }

    int32 Find_CurrentWeaponTooltipSlotIndex(const string& characterId, const string& weaponId)
    {
        for (size_t i = 0; i < kCurrentWeaponTooltipSlots.size(); ++i)
        {
            const CurrentWeaponTooltipSlotDef& slot = kCurrentWeaponTooltipSlots[i];
            if (slot.characterId == characterId && slot.weaponId == weaponId)
                return static_cast<int32>(i);
        }

        return -1;
    }

    bool Starts_With(const wstring& value, const wstring& prefix)
    {
        return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
    }

    bool Ends_With(const wstring& value, const wstring& suffix)
    {
        return value.size() >= suffix.size() &&
               value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    Shared<UI_Text> Find_TooltipTextByPrefixSuffix(
        const umap<wstring, Shared<UI_Base>>& uiByName,
        const wstring& prefix,
        const wstring& suffix)
    {
        for (const auto& [uiName, ui] : uiByName)
        {
            if (!Starts_With(uiName, prefix) || !Ends_With(uiName, suffix))
                continue;

            if (const Shared<UI_Text> text = dynamic_pointer_cast<UI_Text>(ui))
                return text;
        }

        return nullptr;
    }

    void Set_GradeText(const Shared<UI_Text>& text, const string& grade)
    {
        if (text)
            text->Set_Text(wstring(grade.begin(), grade.end()));
    }

    void Apply_MilestoneLock(const Shared<UI_Base>& lockRoot, uint32 displayLevel, uint32 milestoneLevel)
    {
        if (lockRoot == nullptr)
            return;

        const bool shouldShow = displayLevel < milestoneLevel;
        lockRoot->Set_AlphaMultiplier(shouldShow ? 1.f : 0.f);

        if (shouldShow && !lockRoot->Is_Open())
            lockRoot->Open();
        else if (!shouldShow && lockRoot->Is_Open())
            lockRoot->Close();
    }

    constexpr Vec4 kWeaponFrameDefaultTint{ 1.f, 1.f, 1.f, 1.f };
    constexpr Vec4 kWeaponFrameEquippedTint{ 0.78f, 0.36f, 0.58f, 1.f };

    bool Should_PlayWeaponIntroOnEquip(HeroId heroId)
    {
        return heroId == HeroId::Gustave || heroId == HeroId::Maelle;
    }
}

HRESULT ChangeWeapon_Panel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately(false);

    return S_OK;
}

void ChangeWeapon_Panel::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_Open())
        return;

    Handle_WeaponSelectionInput();
}

void ChangeWeapon_Panel::Open()
{
    if (_root == nullptr)
        return;

    if (Is_Open())
        return;

    UI_RuntimePanelBase::Open();
    Set_UIMapOpenState(_uiByName, true);
    Close_ChangeWeaponBlackBackground(_uiByName);

    if (_selectedHeroId == HeroId::END)
        _selectedHeroId = HeroId::Lune;

    CLIENT->Begin_Preview(PreviewShotPanel::ChangeWeapon);
    Show_ChangeWeaponPreview(_selectedHeroId);
	CLIENT->Play_PreviewAnimation(_selectedHeroId, PreviewAnimState::Weapon_Intro, PreviewAnimState::Weapon_Idle);

    Update_PreviewHeroVisibility();
    Apply_PreviewOverrides();
    Refresh_WeaponUI();

    GAME->Play_SoundCue(Constants::SoundCue::UI_Global_GenericEnter);
    GAME->Unlock_Mouse();
    GAME->Set_GameplayPaused(true); // preview 패널 동안 월드 진행을 멈춘다.
}

void ChangeWeapon_Panel::Close()
{
    Close_Immediately(true);
}

void ChangeWeapon_Panel::Close_Immediately(bool restoreWorldControl)
{
    _isProgressionContentMode = false;
    Show_CurrentWeaponTooltip("", "", false);
    UI_RuntimePanelBase::Close();
    Set_UIMapOpenState(_uiByName, false);
    Restore_PreviewOverrides();
    CLIENT->Hide_Preview();

    if (restoreWorldControl)
    {
        GAME->Set_GameplayPaused(false);
        GAME->Lock_Mouse();
    }
}

void ChangeWeapon_Panel::Close_ForMenuTransition()
{
    // 같은 메뉴 스택 안 전환이므로 월드 입력을 복구하지 않는다.
    Close_Immediately(false);
}

void ChangeWeapon_Panel::Open_AsProgressionContent(HeroId selectedHeroId)
{
    if (_root == nullptr)
        return;

    _isProgressionContentMode = true;
    _selectedHeroId = (selectedHeroId == HeroId::END) ? HeroId::Lune : selectedHeroId;

    if (!Is_Open())
        UI_RuntimePanelBase::Open();

    Set_UIMapOpenState(_uiByName, true);
    Close_ChangeWeaponBlackBackground(_uiByName);
    Refresh_ForSelectedHero(_selectedHeroId);
}

void ChangeWeapon_Panel::Close_AsProgressionContent()
{
    if (!_isProgressionContentMode && !Is_Open())
        return;

    _isProgressionContentMode = false;
    Show_CurrentWeaponTooltip("", "", false);
    UI_RuntimePanelBase::Close();
    Set_UIMapOpenState(_uiByName, false);
}

void ChangeWeapon_Panel::Refresh_ForSelectedHero(HeroId selectedHeroId)
{
    _selectedHeroId = (selectedHeroId == HeroId::END) ? HeroId::Lune : selectedHeroId;
    Refresh_WeaponUI();
}

void ChangeWeapon_Panel::Append_RuntimeUIEntries(
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
    rootEntry.editorTarget = const_cast<ChangeWeapon_Panel*>(this);
    outEntries.push_back(rootEntry);

    Append_RuntimeUIMapEntries(outEntries, category, _uiByName, _root, const_cast<ChangeWeapon_Panel*>(this));
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

    umap<wstring, Shared<UI_Base>> tooltipUIByName{};
    UI_RuntimeLoader::UI_LoadDesc tooltipDesc{};
    tooltipDesc.levelIndex = levelIndex;
    tooltipDesc.jsonPath = Constants::UI::UI_ChangeWeapon_Current_Weapon_Tooltip_Path;
    tooltipDesc.prototypeTagPrefix = L"ChangeWeaponCurrentWeaponTooltip";
    tooltipDesc.outUIByName = &tooltipUIByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(tooltipDesc), E_FAIL);

    for (auto& [name, ui] : tooltipUIByName)
        _uiByName[name] = ui;

    return S_OK;
}

HRESULT ChangeWeapon_Panel::Bind_UI()
{
    _weaponRows[0].button = Find_TypedUI<UI_Button>(_uiByName, L"Button_Weapon_First_Slot");
    _weaponRows[0].background = Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_First_Slot_BG");
    _weaponRows[0].weaponNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Weapon_First_Slot_Name");
    _weaponRows[0].levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Weapon_First_Slot_Level_Value");
    _weaponRows[0].icons =
    {
        WeaponRowIconUI{ "Gustave", "Lanceram", Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_First_Slot_Gustave_Lanceram") },
        WeaponRowIconUI{ "Lune",    "Colim",    Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_First_Slot_Lune_Colim") },
        WeaponRowIconUI{ "Maelle",  "Medalum",  Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_First_Slot_Maelle_Medalum") },
    };

    _weaponRows[1].button = Find_TypedUI<UI_Button>(_uiByName, L"Button_Weapon_Second_Slot");
    _weaponRows[1].background = Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_Second_Slot_BG");
    _weaponRows[1].weaponNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Weapon_Second_Slot_Name");
    _weaponRows[1].levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Weapon_Second_Slot_Level_Value");
    _weaponRows[1].icons =
    {
        WeaponRowIconUI{ "Gustave", "Baguette", Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_Second_Slot_Gustave_Baguette") },
        WeaponRowIconUI{ "Lune",    "Baguette", Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_Second_Slot_Lune_Baguette") },
        WeaponRowIconUI{ "Maelle",  "Baguette", Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_Second_Slot_Maelle_Baguette") },
    };

    _weaponRows[2].button = Find_TypedUI<UI_Button>(_uiByName, L"Button_Weapon_Third_Slot");
    _weaponRows[2].background = Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_Third_Slot_BG");
    _weaponRows[2].weaponNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Weapon_Third_Slot_Name");
    _weaponRows[2].levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Weapon_Third_Slot_Level_Value");
    _weaponRows[2].icons =
    {
        WeaponRowIconUI{ "Gustave", "Tireso", Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_Third_Slot_Gustave_Tireso") },
        WeaponRowIconUI{ "Lune",    "Snowim", Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_Third_Slot_Lune_Snowim") },
        WeaponRowIconUI{ "Maelle",  "Plenum", Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_Third_Slot_Maelle_Plenum") },
    };

    for (size_t i = 0; i < WeaponSlotCount; ++i)
    {
        CHECK_NULL(_weaponRows[i].button, E_FAIL);
        CHECK_NULL(_weaponRows[i].background, E_FAIL);
        CHECK_NULL(_weaponRows[i].weaponNameText, E_FAIL);
        CHECK_NULL(_weaponRows[i].levelText, E_FAIL);

        for (const WeaponRowIconUI& icon : _weaponRows[i].icons)
            CHECK_NULL(icon.icon, E_FAIL);
    }

    CHECK_FAILED(Bind_CurrentWeaponTooltipUI(), E_FAIL);

    return S_OK;
}

HRESULT ChangeWeapon_Panel::Bind_CurrentWeaponTooltipUI()
{
    _currentWeaponTooltipRoot = Find_UI(_uiByName, L"Panel_ChangeWeapon_Current_Weapon_Tooltip");
    CHECK_NULL(_currentWeaponTooltipRoot, E_FAIL);

    for (size_t i = 0; i < _currentWeaponTooltips.size(); ++i)
    {
        const wstring token = kCurrentWeaponTooltipSlots[i].tooltipToken;
        CurrentWeaponTooltipUI& ui = _currentWeaponTooltips[i];

        ui.root = Find_UI(_uiByName, L"Panel_" + token + L"_Tooltip");
        ui.weaponNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_" + token + L"_Tooltip");
        ui.levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_" + token + L"_Tooltip_Current_" + token + L"_Level_Value");
        if (ui.levelText == nullptr)
        {
            ui.levelText = Find_TooltipTextByPrefixSuffix(
                _uiByName,
                L"Text_" + token + L"_Tooltip_Current_",
                L"_Level_Value");
        }
        ui.powerText = Find_TypedUI<UI_Text>(_uiByName, L"Text_" + token + L"_Tooltip_Power_Value");
        ui.vitalityGradeText = Find_TypedUI<UI_Text>(_uiByName, L"Text_" + token + L"_Tooltip_Vitality_Value");
        ui.mightGradeText = Find_TypedUI<UI_Text>(_uiByName, L"Text_" + token + L"_Tooltip_Might_Value");
        ui.agilityGradeText = Find_TypedUI<UI_Text>(_uiByName, L"Text_" + token + L"_Tooltip_Agility_Value");
        ui.defenseGradeText = Find_TypedUI<UI_Text>(_uiByName, L"Text_" + token + L"_Tooltip_Defense_Value");
        ui.luckGradeText = Find_TypedUI<UI_Text>(_uiByName, L"Text_" + token + L"_Tooltip_Luck_Value");
        ui.thumbnail = Find_TypedUI<UI_Image>(_uiByName, L"Image_" + token + L"_Thumnail_Tooltip");
        ui.thumbnailBackground = Find_TypedUI<UI_Image>(_uiByName, L"Image_" + token + L"_Thumnail_BG_Tooltip");
        ui.level5Lock = Find_UI(_uiByName, L"Panel_" + token + L"_Level_5_Tooltip_Lock");
        ui.level8Lock = Find_UI(_uiByName, L"Panel_" + token + L"_Level_8_Tooltip_Lock");

        CHECK_NULL(ui.root, E_FAIL);
        CHECK_NULL(ui.weaponNameText, E_FAIL);
        CHECK_NULL(ui.levelText, E_FAIL);
        CHECK_NULL(ui.powerText, E_FAIL);
        CHECK_NULL(ui.thumbnail, E_FAIL);
        CHECK_NULL(ui.thumbnailBackground, E_FAIL);
    }

    return S_OK;
}

void ChangeWeapon_Panel::Handle_WeaponSelectionInput()
{
    const HeroId heroId = (_selectedHeroId == HeroId::END) ? HeroId::Lune : _selectedHeroId;
    const string characterId = Get_CharacterId(heroId);
    const vector<GameDataTable::WeaponDesc>* weapons = CLIENT->Get_WeaponDescs(characterId);
    if (weapons == nullptr || weapons->empty())
        return;

    for (size_t i = 0; i < WeaponSlotCount; ++i)
    {
        WeaponRowUI& row = _weaponRows[i];
        if (row.button == nullptr || i >= weapons->size())
            continue;

        if (!row.button->Was_Clicked())
            continue;

        _selectedWeaponIndex = i;

        const GameDataTable::WeaponDesc& selectedWeapon = (*weapons)[_selectedWeaponIndex];
        const auto inventory = CLIENT->Get_PlayerInventory();
        if (inventory && !inventory->Is_WeaponUnlocked(characterId, selectedWeapon.weaponId))
            continue; // 숨겨진/미소유 무기가 클릭되어도 장착 소비 위치에서 한 번 더 막는다.

        const auto hero = CLIENT->Find_Hero(heroId);
        const auto statCom = hero ? hero->Get_Component<PlayerStatCom>() : nullptr;
        const bool weaponChanged = statCom &&
            statCom->Get_PlayerRuntimeStat().CurEquippedWeaponId != selectedWeapon.weaponId;
        if (statCom)
        {
            statCom->Set_EquippedWeapon(
                selectedWeapon.weaponId,
                CLIENT->Resolve_WeaponPower(characterId, selectedWeapon.weaponId));
        }

        CLIENT->Sync_PreviewHeroRuntimeData(heroId);
        Apply_ChangeWeaponPreviewWeapon(heroId);
        if (weaponChanged && Should_PlayWeaponIntroOnEquip(heroId))
            CLIENT->Play_PreviewAnimation(heroId, PreviewAnimState::Weapon_Intro, PreviewAnimState::Weapon_Idle);

        Refresh_WeaponUI(); // 장착 row 보라색 표시와 selected index를 새 장착값 기준으로 다시 맞춘다.

        GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_SelectAlly);
        break;
    }
}

void ChangeWeapon_Panel::Refresh_WeaponUI()
{
    const HeroId heroId = (_selectedHeroId == HeroId::END) ? HeroId::Lune : _selectedHeroId;
    if (Is_Open() && !_isProgressionContentMode)
    {
        Show_ChangeWeaponPreview(heroId);
        Update_PreviewHeroVisibility();
        Apply_PreviewOverrides();
    }

    const Shared<Player_WorldBase> hero = CLIENT->Find_Hero(heroId);
    const Shared<PlayerStatCom> statCom = hero ? hero->Get_Component<PlayerStatCom>() : nullptr;

    const string characterId = Get_CharacterId(heroId);
    const vector<GameDataTable::WeaponDesc>* weapons = CLIENT->Get_WeaponDescs(characterId);
    if (weapons == nullptr || weapons->empty())
    {
        LOG_WARN("ChangeWeaponPanel weapon data not found: {}", characterId);

        // 무기 데이터가 없으면 이전 hero row가 남지 않게 전부 닫는다.
        for (WeaponRowUI& row : _weaponRows)
        {
            Close_WeaponRowIcons(row);

            if (row.button)
                row.button->Close();
        }

        Show_CurrentWeaponTooltip("", "", false);
        return;
    }

    const bool defaultEquipped = CLIENT->Ensure_DefaultEquippedWeapon(heroId);
    if (defaultEquipped)
        CLIENT->Sync_PreviewHeroRuntimeData(heroId); // 기본 장착이 새로 생긴 경우 preview 무기도 즉시 맞춘다.

    Apply_ChangeWeaponPreviewWeapon(heroId);

    const string equippedWeaponId = statCom ? statCom->Get_PlayerRuntimeStat().CurEquippedWeaponId : "";

    _selectedWeaponIndex = 0;
    for (size_t i = 0; i < weapons->size(); ++i)
    {
        if ((*weapons)[i].weaponId == equippedWeaponId)
        {
            _selectedWeaponIndex = i; // tooltip은 현재 장착 무기를 기본 선택값으로 맞춘다.
            break;
        }
    }

    if (_selectedWeaponIndex >= weapons->size())
        _selectedWeaponIndex = 0;

    Refresh_WeaponRows(*weapons, equippedWeaponId);

    const GameDataTable::WeaponDesc& currentWeapon = (*weapons)[_selectedWeaponIndex];
    Refresh_CurrentWeaponTooltip(characterId, currentWeapon);
    Show_CurrentWeaponTooltip(characterId, currentWeapon.weaponId, true);
}

void ChangeWeapon_Panel::Refresh_CurrentWeaponTooltip(const string& characterId, const GameDataTable::WeaponDesc& weapon)
{
    const int32 tooltipIndex = Find_CurrentWeaponTooltipSlotIndex(characterId, weapon.weaponId);
    if (tooltipIndex < 0 || tooltipIndex >= static_cast<int32>(_currentWeaponTooltips.size()))
        return;

    CurrentWeaponTooltipUI& ui = _currentWeaponTooltips[tooltipIndex];
    const uint32 currentLevel = CLIENT->Resolve_WeaponEnhanceLevel(characterId, weapon.weaponId);
    const uint32 currentPower = CLIENT->Resolve_WeaponPower(characterId, weapon.weaponId);
    const GameDataTable::WeaponUpgradeDesc* upgrade =
        CLIENT->Get_WeaponUpgradeDesc(characterId, weapon.weaponId, currentLevel);

    if (ui.weaponNameText)
        ui.weaponNameText->Set_Text(weapon.weaponName);

    if (ui.levelText)
        ui.levelText->Set_Text(to_wstring(currentLevel));

    if (ui.powerText)
        ui.powerText->Set_Text(to_wstring(currentPower));

    Set_GradeText(ui.vitalityGradeText, upgrade ? upgrade->vitalityGrade : "");
    Set_GradeText(ui.mightGradeText, upgrade ? upgrade->mightGrade : "");
    Set_GradeText(ui.agilityGradeText, upgrade ? upgrade->agilityGrade : "");
    Set_GradeText(ui.defenseGradeText, upgrade ? upgrade->defenseGrade : "");
    Set_GradeText(ui.luckGradeText, upgrade ? upgrade->luckGrade : "");

    Apply_MilestoneLock(ui.level5Lock, currentLevel, 5);
    Apply_MilestoneLock(ui.level8Lock, currentLevel, 8);
}

void ChangeWeapon_Panel::Show_CurrentWeaponTooltip(const string& characterId, const string& weaponId, bool isVisible)
{
    Close_AllCurrentWeaponTooltips();

    if (_currentWeaponTooltipRoot == nullptr)
        return;

    if (!isVisible)
    {
        _currentWeaponTooltipRoot->Close();
        return;
    }

    const int32 tooltipIndex = Find_CurrentWeaponTooltipSlotIndex(characterId, weaponId);
    if (tooltipIndex < 0 || tooltipIndex >= static_cast<int32>(_currentWeaponTooltips.size()))
    {
        _currentWeaponTooltipRoot->Close();
        return;
    }

    if (!_currentWeaponTooltipRoot->Is_Open())
        _currentWeaponTooltipRoot->Open();

    Shared<UI_Base> tooltipRoot = _currentWeaponTooltips[tooltipIndex].root;
    if (tooltipRoot == nullptr)
        return;

    tooltipRoot->Set_AlphaMultiplier(1.f);
    if (!tooltipRoot->Is_Open())
        tooltipRoot->Open();
}

void ChangeWeapon_Panel::Close_AllCurrentWeaponTooltips()
{
    for (CurrentWeaponTooltipUI& tooltip : _currentWeaponTooltips)
    {
        if (tooltip.root == nullptr)
            continue;

        tooltip.root->Set_AlphaMultiplier(0.f);
        tooltip.root->Close();
    }
}

void ChangeWeapon_Panel::Close_WeaponRowIcons(WeaponRowUI& row)
{
    for (WeaponRowIconUI& icon : row.icons)
    {
        if (icon.icon)
            icon.icon->Close();
    }
}

Shared<UI_Image> ChangeWeapon_Panel::Find_WeaponRowIcon(WeaponRowUI& row, const string& characterId, const string& weaponId)
{
    for (WeaponRowIconUI& icon : row.icons)
    {
        if (icon.characterId == characterId && icon.weaponId == weaponId)
            return icon.icon;
    }

    return nullptr;
}

void ChangeWeapon_Panel::Refresh_WeaponRows(const vector<GameDataTable::WeaponDesc>& weapons, const string& equippedWeaponId)
{
    const auto inventory = CLIENT->Get_PlayerInventory();
    const string characterId = Get_CharacterId((_selectedHeroId == HeroId::END) ? HeroId::Lune : _selectedHeroId);

    for (size_t i = 0; i < WeaponSlotCount; ++i)
    {
        WeaponRowUI& row = _weaponRows[i];
        if (row.button == nullptr)
            continue;

        if (i >= weapons.size())
        {
            Close_WeaponRowIcons(row);
            row.button->Close();
            continue;
        }

        const GameDataTable::WeaponDesc& weapon = weapons[i];
        const bool isOwned = inventory == nullptr || inventory->Is_WeaponUnlocked(characterId, weapon.weaponId);
        if (!isOwned)
        {
            Close_WeaponRowIcons(row);
            row.button->Close(); // 아직 소유하지 않은 무기는 ChangeWeapon 선택 목록에서 숨긴다.
            continue;
        }

        row.button->Open();
        Close_WeaponRowIcons(row);

        if (Shared<UI_Image> icon = Find_WeaponRowIcon(row, characterId, weapon.weaponId))
            icon->Open();

        if (row.background)
        {
            const bool isEquipped = weapon.weaponId == equippedWeaponId;
            row.background->Set_Color(isEquipped ? kWeaponFrameEquippedTint : kWeaponFrameDefaultTint); // ChangeSkill처럼 장착된 row를 보라색으로 표시한다.
        }

        if (row.weaponNameText)
            row.weaponNameText->Set_Text(weapon.weaponName);

        if (row.levelText)
            row.levelText->Set_Text(to_wstring(CLIENT->Resolve_WeaponEnhanceLevel(characterId, weapon.weaponId)));
    }
}

string ChangeWeapon_Panel::Get_CharacterId(HeroId heroId)
{
    switch (heroId)
    {
    case HeroId::Gustave:
        return "Gustave";

    case HeroId::Lune:
        return "Lune";

    case HeroId::Maelle:
        return "Maelle";

    case HeroId::END:
    default:
        return "Lune";
    }
}

void ChangeWeapon_Panel::Apply_PreviewOverrides()
{
    const Shared<PostProcessPipeline> postProcessPipeline = GAME->Get_PostProcessPipeline();
    const Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();
    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    const LIGHT_DESC* primaryLightDesc = GAME->Get_PrimaryShadowLightDesc();
    const ShadowLightDesc* shadowLightDesc = GAME->Get_ShadowLightDesc();
    if (primaryLightDesc == nullptr)
    {
        const uint32 lightCount = GAME->Get_LightCount();
        for (uint32 lightIndex = 0; lightIndex < lightCount; ++lightIndex)
        {
            const LIGHT_DESC* lightDesc = GAME->Get_LightDesc(lightIndex);
            if (lightDesc == nullptr || lightDesc->type != LightType::Directional)
                continue;

            primaryLightDesc = lightDesc;
            break;
        }
    }

    if (previewManager == nullptr)
        return;

    if (_previewOverrideState.isCaptured == false)
    {
        _previewOverrideState.isCaptured = true;
        _previewOverrideState.cascadeLevel = GAME->Get_ShadowCascadeLevel();
        _previewOverrideState.hasPrimaryLight = (primaryLightDesc != nullptr);
        _previewOverrideState.hasShadowLight = (shadowLightDesc != nullptr);
        _previewOverrideState.hasPostProcess = (postProcessPipeline != nullptr);
        _previewOverrideState.hasLighting = (lightingPipeline != nullptr);

        if (primaryLightDesc != nullptr)
            _previewOverrideState.primaryLight = *primaryLightDesc;

        if (shadowLightDesc != nullptr)
            _previewOverrideState.shadowLight = *shadowLightDesc;

        if (postProcessPipeline != nullptr)
        {
            _previewOverrideState.postProcess = postProcessPipeline->Get_PostProcessParams();
            _previewOverrideState.postProcessFeatures = postProcessPipeline->Get_PostProcessFeatures();
        }

        if (lightingPipeline != nullptr)
        {
            _previewOverrideState.deferredLighting = lightingPipeline->Get_DeferredLightingParams();
            _previewOverrideState.iblEnabled = lightingPipeline->Is_IBLEnabled();
        }
    }

    if (_previewOverrideState.hasPrimaryLight)
    {
        LIGHT_DESC previewLight = _previewOverrideState.primaryLight;
        Vec3 previewLightDir = kPreviewLightDirection;
        if (previewLightDir.LengthSquared() > 0.0001f)
            previewLightDir.Normalize();

        previewLight.direction = Vec4(previewLightDir.x, previewLightDir.y, previewLightDir.z, 0.f);
        previewLight.diffuse = kPreviewLightDiffuse;
        previewLight.ambient = kPreviewLightAmbient;
        previewLight.specular = kPreviewLightSpecular;
        GAME->Update_PrimaryShadowLightDesc(previewLight);
        GAME->Set_ShadowCascadeLevel(Engine::CascadeLevel::Close);
    }

    if (_previewOverrideState.hasShadowLight)
    {
        ShadowLightDesc previewShadow = _previewOverrideState.shadowLight;
        previewShadow.intensity = max(previewShadow.intensity, kPreviewShadowIntensity);
        GAME->Update_ShadowLight(previewShadow);
    }

    if (_previewOverrideState.hasPostProcess && postProcessPipeline != nullptr)
    {
        Engine::PostProcessCB previewPostProcess = _previewOverrideState.postProcess;
        previewPostProcess.exposure = 0.60f;
        previewPostProcess.gamma = 1.33f;
        previewPostProcess.bloomThreshold = 0.55f;
        previewPostProcess.bloomIntensity = 0.95f;
        previewPostProcess.bloomScatter = 0.92f;
        postProcessPipeline->Set_PostProcessFeatures(
            Engine::PostProcessFeature::HDR |
            Engine::PostProcessFeature::ToneMapping |
            Engine::PostProcessFeature::Exposure |
            Engine::PostProcessFeature::Bloom |
            Engine::PostProcessFeature::Gamma);
        postProcessPipeline->Set_PostProcessParams(previewPostProcess);
    }

    if (_previewOverrideState.hasLighting && lightingPipeline != nullptr)
    {
        Engine::DeferredLightingCB previewDeferredLighting = _previewOverrideState.deferredLighting;
        previewDeferredLighting.iblIntensity = 0.64f;
        previewDeferredLighting.iblDiffuseIntensity = 0.34f;
        previewDeferredLighting.iblSpecularIntensity = 1.18f;
        lightingPipeline->Set_DeferredLightingParams(previewDeferredLighting);
        lightingPipeline->Set_IBLEnabled(true);
    }

    Apply_PreviewRimState();
}

void ChangeWeapon_Panel::Restore_PreviewOverrides()
{
    if (!_previewOverrideState.isCaptured)
        return;

    Clear_PreviewRimState();

    const Shared<PostProcessPipeline> postProcessPipeline = GAME->Get_PostProcessPipeline();
    const Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();

    if (_previewOverrideState.hasPrimaryLight)
    {
        GAME->Update_PrimaryShadowLightDesc(_previewOverrideState.primaryLight);
        GAME->Set_ShadowCascadeLevel(_previewOverrideState.cascadeLevel);
    }

    if (_previewOverrideState.hasShadowLight)
        GAME->Update_ShadowLight(_previewOverrideState.shadowLight);

    if (_previewOverrideState.hasPostProcess && postProcessPipeline != nullptr)
    {
        postProcessPipeline->Set_PostProcessFeatures(_previewOverrideState.postProcessFeatures);
        postProcessPipeline->Set_PostProcessParams(_previewOverrideState.postProcess);
    }

    if (_previewOverrideState.hasLighting && lightingPipeline != nullptr)
    {
        lightingPipeline->Set_DeferredLightingParams(_previewOverrideState.deferredLighting);
        lightingPipeline->Set_IBLEnabled(_previewOverrideState.iblEnabled);
    }

    _previewOverrideState = PreviewOverrideState{};
}

void ChangeWeapon_Panel::Apply_PreviewRimState()
{
    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    if (previewManager == nullptr)
        return;

    for (const auto& previewHero : previewManager->Get_PreviewHeroes())
    {
        if (previewHero.player == nullptr)
            continue;

        const Shared<EffectCom> effectCom = previewHero.player->Get_Component<EffectCom>();
        if (effectCom == nullptr)
            continue;

        if (previewHero.player->Is_Visible())
            effectCom->Set_PreviewRim(
                kPreviewRimColor,
                kPreviewRimIntensity,
                kPreviewRimPower,
                kPreviewHairRimIntensity);
        else
            effectCom->Clear_PreviewRim();
    }
}

void ChangeWeapon_Panel::Clear_PreviewRimState()
{
    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    if (previewManager == nullptr)
        return;

    for (const auto& previewHero : previewManager->Get_PreviewHeroes())
    {
        if (previewHero.player == nullptr)
            continue;

        if (const Shared<EffectCom> effectCom = previewHero.player->Get_Component<EffectCom>())
            effectCom->Clear_PreviewRim();
    }
}

void ChangeWeapon_Panel::Update_PreviewHeroVisibility()
{
    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    if (previewManager == nullptr)
        return;

    for (const auto& previewHero : previewManager->Get_PreviewHeroes())
    {
        if (previewHero.player == nullptr)
            continue;

        previewHero.player->Set_Visible(previewHero.heroId == _selectedHeroId);
    }
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
    if (Is_Open())
        Close_Immediately(false);

    _isProgressionContentMode = false;
    _weaponRows = {};
    _currentWeaponTooltipRoot.reset();
    _currentWeaponTooltips = {};

    UI_RuntimeLoader::Release_Document(_uiByName);
    _root.reset();

    UI_RuntimePanelBase::Free();
}
