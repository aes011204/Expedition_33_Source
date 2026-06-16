#include "ChangeWeapon_Panel.h"

#include "Client_Constants.h"
#include "ClientInstance.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "LightingPipeline.h"
#include "Player_WorldBase.h"
#include "PlayerStatCom.h"
#include "PostProcessPipeline.h"
#include "PreviewRimFeature.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UIPreviewManager.h"
#include "UI_Utils.h"

#include <algorithm>

using namespace UI_Utils;

namespace
{
    constexpr const wchar_t* kRoundWeaponBgPath = L"../Bin/Resources/Textures/UI/Inventory/Weapons/Background/T_UI_RoundWeaponBackground.png";
    constexpr const wchar_t* kLongWeaponBgPath = L"../Bin/Resources/Textures/UI/Inventory/Weapons/Background/T_UI_LongWeaponBackground.png";

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
        CLIENT->Show_Preview(heroId, PreviewShotPanel::SkillTree);
    }

    wstring Resolve_WeaponBackgroundTexturePath(const string& weaponId)
    {
        if (weaponId == "Colim" || weaponId == "Esquim")
            return kRoundWeaponBgPath;

        if (weaponId == "Baguette" ||
            weaponId == "Lanceram" ||
            weaponId == "Candy" ||
            weaponId == "Medalum" ||
            weaponId == "Sekarum")
            return kLongWeaponBgPath;

        return L"";
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

    Handle_SelectionInput();
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

    if (_selectedHeroId == HeroId::END)
        _selectedHeroId = HeroId::Lune;

    CLIENT->Begin_Preview(PreviewShotPanel::SkillTree);
    Show_ChangeWeaponPreview(_selectedHeroId);
    Update_PreviewHeroVisibility();
    Apply_PreviewOverrides();
    Refresh_WeaponUI();
    Show_WeaponTooltip(false); // ChangeWeapon 진입 직후 오른쪽 재료/설명 tooltip은 숨긴다.

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
    _characterButtons[0] = Find_TypedUI<UI_Button>(_uiByName, L"Button_CharacterSelect_01");
    _characterButtons[1] = Find_TypedUI<UI_Button>(_uiByName, L"Button_CharacterSelect_02");
    _characterButtons[2] = Find_TypedUI<UI_Button>(_uiByName, L"Button_CharacterSelect_03");

    _characterStains[0] = Find_TypedUI<UI_Image>(_uiByName, L"Image_CharacterSelectButton_Stain_01");
    _characterStains[1] = Find_TypedUI<UI_Image>(_uiByName, L"Image_CharacterSelectButton_Stain_02");
    _characterStains[2] = Find_TypedUI<UI_Image>(_uiByName, L"Image_CharacterSelectButton_Stain_03");

    _tooltipText.weaponNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_WeaponName");
    _tooltipText.levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentWeaponLevel");
    _tooltipText.powerText = Find_TypedUI<UI_Text>(_uiByName, L"Text_WeaponTooltip_PowerValue");
    _characterNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Character_Name");
    _currentLevelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentLevel");
    _currentExpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Character_Current_EXP");
    _maxExpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Character_Max_EXP");
    _currentHpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Character_Current_HP");
    _maxHpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Character_Max_HP");
    _currentStatHpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatHP");
    _currentStatAttackText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatAttack");
    _currentStatDefenseText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatDefense");
    _currentStatSpeedText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatSpeed");
    _currentStatCriticalPercentText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatCriticalPercent");
    _expGaugeImage = Find_TypedUI<UI_Image>(_uiByName, L"Text_Character_EXP_Gauge");
    _hpGaugeImage = Find_TypedUI<UI_Image>(_uiByName, L"Text_Character_HP_Gauge");
    _weaponTooltipRoot = Find_UI(_uiByName, L"Panel_WeaponTooltip");

    _weaponRows[0].button = Find_TypedUI<UI_Button>(_uiByName, L"Button_Lune_Baguette");
    _weaponRows[0].background = Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_BG_001");
    _weaponRows[0].icon = Find_TypedUI<UI_Image>(_uiByName, L"Image_Lune_Baguette");
    _weaponRows[0].weaponNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Lune_Baguette");
    _weaponRows[0].levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Weapon_LevelValue");

    _weaponRows[1].button = Find_TypedUI<UI_Button>(_uiByName, L"Button_Lune_Snowim");
    _weaponRows[1].background = Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_BG_002");
    _weaponRows[1].icon = Find_TypedUI<UI_Image>(_uiByName, L"Image_Lune_Snowim");
    _weaponRows[1].weaponNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Lune_Snowim");
    _weaponRows[1].levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Weapon_LevelValue_001");

    _weaponRows[2].button = Find_TypedUI<UI_Button>(_uiByName, L"Button_Lune_Esquim");
    _weaponRows[2].background = Find_TypedUI<UI_Image>(_uiByName, L"Image_Weapon_BG_003");
    _weaponRows[2].icon = Find_TypedUI<UI_Image>(_uiByName, L"Image_Lune_Esquim");
    _weaponRows[2].weaponNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Lune_Esquim");
    _weaponRows[2].levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Weapon_LevelValue_002");

    _tooltipDetailUI.abilityValueText1 = Find_TypedUI<UI_Text>(_uiByName, L"Text_WeaponTooltip_AbilityValue_1");
    _tooltipDetailUI.abilityValueText2 = Find_TypedUI<UI_Text>(_uiByName, L"Text_WeaponTooltip_AbilityValue_2");

    _tooltipDetailUI.levelTexts[0] = Find_TypedUI<UI_Text>(_uiByName, L"Text_WeaponTooltip_Level");
    _tooltipDetailUI.levelTexts[1] = Find_TypedUI<UI_Text>(_uiByName, L"Text_WeaponTooltip_Level_001");
    _tooltipDetailUI.levelTexts[2] = Find_TypedUI<UI_Text>(_uiByName, L"Text_WeaponTooltip_Level_002");

    _tooltipDetailUI.descriptionTexts[0] = Find_TypedUI<UI_Text>(_uiByName, L"Text_WaeponTooltip_Description");
    _tooltipDetailUI.descriptionTexts[1] = Find_TypedUI<UI_Text>(_uiByName, L"Text_WaeponTooltip_Description_001");
    _tooltipDetailUI.descriptionTexts[2] = Find_TypedUI<UI_Text>(_uiByName, L"Text_WaeponTooltip_Description_002");

    CHECK_NULL(_characterButtons[0], E_FAIL);
    CHECK_NULL(_characterButtons[1], E_FAIL);
    CHECK_NULL(_characterButtons[2], E_FAIL);
    CHECK_NULL(_characterStains[0], E_FAIL);
    CHECK_NULL(_characterStains[1], E_FAIL);
    CHECK_NULL(_characterStains[2], E_FAIL);
    CHECK_NULL(_tooltipText.weaponNameText, E_FAIL);
    CHECK_NULL(_tooltipText.levelText, E_FAIL);
    CHECK_NULL(_tooltipText.powerText, E_FAIL);
    CHECK_NULL(_characterNameText, E_FAIL);
    CHECK_NULL(_currentLevelText, E_FAIL);
    CHECK_NULL(_currentExpText, E_FAIL);
    CHECK_NULL(_maxExpText, E_FAIL);
    CHECK_NULL(_currentHpText, E_FAIL);
    CHECK_NULL(_maxHpText, E_FAIL);
    CHECK_NULL(_currentStatHpText, E_FAIL);
    CHECK_NULL(_currentStatAttackText, E_FAIL);
    CHECK_NULL(_currentStatDefenseText, E_FAIL);
    CHECK_NULL(_currentStatSpeedText, E_FAIL);
    CHECK_NULL(_currentStatCriticalPercentText, E_FAIL);
    CHECK_NULL(_expGaugeImage, E_FAIL);
    CHECK_NULL(_hpGaugeImage, E_FAIL);
    CHECK_NULL(_weaponTooltipRoot, E_FAIL);

    CHECK_NULL(_tooltipDetailUI.abilityValueText1, E_FAIL);
    CHECK_NULL(_tooltipDetailUI.abilityValueText2, E_FAIL);

    CHECK_NULL(_tooltipDetailUI.levelTexts[0], E_FAIL);
    CHECK_NULL(_tooltipDetailUI.levelTexts[1], E_FAIL);
    CHECK_NULL(_tooltipDetailUI.levelTexts[2], E_FAIL);

    CHECK_NULL(_tooltipDetailUI.descriptionTexts[0], E_FAIL);
    CHECK_NULL(_tooltipDetailUI.descriptionTexts[1], E_FAIL);
    CHECK_NULL(_tooltipDetailUI.descriptionTexts[2], E_FAIL);

    for (size_t i = 0; i < WeaponSlotCount; ++i)
    {
        CHECK_NULL(_weaponRows[i].button, E_FAIL);
        CHECK_NULL(_weaponRows[i].background, E_FAIL);
        CHECK_NULL(_weaponRows[i].icon, E_FAIL);
        CHECK_NULL(_weaponRows[i].weaponNameText, E_FAIL);
        CHECK_NULL(_weaponRows[i].levelText, E_FAIL);
    }

    _expGaugeImage->Set_UseFillRatio(true); // 경험치 바는 UV crop 방식으로 채운다.
    _hpGaugeImage->Set_UseFillRatio(true);  // HP 바도 같은 방식으로 채운다.

    return S_OK;
}

void ChangeWeapon_Panel::Handle_SelectionInput()
{
    const vector<HeroPartyEntry>& partyEntries = CLIENT->Get_HeroEntries();
    if (partyEntries.empty())
        return;

    HeroId previousHeroId = (_selectedHeroId == HeroId::END) ? partyEntries.front().heroId : _selectedHeroId;

    if (GAME->KeyDown(KEY_TYPE::Q) || GAME->KeyDown(KEY_TYPE::E))
    {
        uint32 currentIndex = 0;
        for (uint32 i = 0; i < static_cast<uint32>(partyEntries.size()); ++i)
        {
            if (partyEntries[i].heroId == previousHeroId)
            {
                currentIndex = i;
                break;
            }
        }

        if (GAME->KeyDown(KEY_TYPE::Q))
            currentIndex = (currentIndex + 1) % static_cast<uint32>(partyEntries.size());
        else
            currentIndex = (currentIndex + static_cast<uint32>(partyEntries.size()) - 1) % static_cast<uint32>(partyEntries.size());


        _selectedHeroId = partyEntries[currentIndex].heroId;
    }

    if (_selectedHeroId == HeroId::END)
        _selectedHeroId = previousHeroId;

    if (_selectedHeroId != previousHeroId)
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_SelectAlly);

        Show_ChangeWeaponPreview(_selectedHeroId);
        Update_PreviewHeroVisibility();
        Apply_PreviewOverrides();

        Refresh_WeaponUI(); // hero가 바뀌면 row visible / tooltip / stain을 같이 갱신한다.
    }
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

        Refresh_Tooltip((*weapons)[_selectedWeaponIndex]);
        Refresh_TooltipDetails((*weapons)[_selectedWeaponIndex]); // 설명/등급/능력치도 같이 갱신
        Show_WeaponTooltip(true);

        GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_SelectAlly);
        break;
    }
}

void ChangeWeapon_Panel::Refresh_CharacterSelectUI()
{
    static constexpr array<HeroId, 3> kHeroOrder =
    {
        HeroId::Lune,
        HeroId::Gustave,
        HeroId::Maelle,
    };

    for (size_t i = 0; i < kHeroOrder.size(); ++i)
    {
        if (_characterStains[i] == nullptr)
            continue;

        if (kHeroOrder[i] == _selectedHeroId)
            _characterStains[i]->Open(); // 현재 선택 hero만 stain을 켠다.
        else
            _characterStains[i]->Close();
    }
}

void ChangeWeapon_Panel::Refresh_CharacterInfo(const Shared<PlayerStatCom>& statCom)
{
    if (_characterNameText)
        _characterNameText->Set_Text(Get_DisplayHeroName(_selectedHeroId));

    if (statCom == nullptr)
    {
        if (_currentLevelText) _currentLevelText->Set_Text(L"0");
        if (_currentExpText) _currentExpText->Set_Text(L"0");
        if (_maxExpText) _maxExpText->Set_Text(L"0");
        if (_currentHpText) _currentHpText->Set_Text(L"0");
        if (_maxHpText) _maxHpText->Set_Text(L"0");
        if (_currentStatHpText) _currentStatHpText->Set_Text(L"0");
        if (_currentStatAttackText) _currentStatAttackText->Set_Text(L"0");
        if (_currentStatDefenseText) _currentStatDefenseText->Set_Text(L"0");
        if (_currentStatSpeedText) _currentStatSpeedText->Set_Text(L"0");
        if (_currentStatCriticalPercentText) _currentStatCriticalPercentText->Set_Text(L"0");

        if (_expGaugeImage)
            _expGaugeImage->Set_FillRatio(0.f);

        if (_hpGaugeImage)
            _hpGaugeImage->Set_FillRatio(0.f);

        return;
    }

    const PlayerStatCom::PRuntimeCombatStat& statInfo = statCom->Get_PlayerRuntimeStat();
    const uint32 maxExp = statInfo.CurMaxExp > 0 ? statInfo.CurMaxExp : max(1u, statInfo.nextExp);
    const float expRatio = std::clamp(
        maxExp > 0 ? static_cast<float>(statInfo.CurExp) / static_cast<float>(maxExp) : 0.f,
        0.f,
        1.f);
    const float hpRatio = std::clamp(
        statInfo.CurMaxHp > 0 ? static_cast<float>(statInfo.CurHp) / static_cast<float>(statInfo.CurMaxHp) : 0.f,
        0.f,
        1.f);

    Set_UIntText(_currentLevelText, statInfo.CurLevel);
    Set_UIntText(_currentExpText, statInfo.CurExp);
    Set_UIntText(_maxExpText, maxExp);
    Set_IntText(_currentHpText, static_cast<int32>(statInfo.CurHp));
    Set_IntText(_maxHpText, static_cast<int32>(statInfo.CurMaxHp));
    Set_IntText(_currentStatHpText, static_cast<int32>(statInfo.CurMaxHp));
    Set_IntText(_currentStatAttackText, static_cast<int32>(statInfo.CurPhysicalAttack));
    Set_IntText(_currentStatDefenseText, static_cast<int32>(statInfo.CurPhysicalDefense));
    Set_IntText(_currentStatSpeedText, static_cast<int32>(statInfo.CurSpeed));
    Set_IntText(_currentStatCriticalPercentText, static_cast<int32>(statInfo.CurCriticalRate));

    if (_expGaugeImage)
        _expGaugeImage->Set_FillRatio(expRatio); // 현재 경험치 / 다음 레벨 필요 경험치 비율

    if (_hpGaugeImage)
        _hpGaugeImage->Set_FillRatio(hpRatio);   // 현재 HP / 최대 HP 비율
}

void ChangeWeapon_Panel::Refresh_WeaponUI()
{
    const HeroId heroId = (_selectedHeroId == HeroId::END) ? HeroId::Lune : _selectedHeroId;
    if (Is_Open())
    {
        Show_ChangeWeaponPreview(heroId); // 무기 UI 갱신 시점과 preview target을 항상 같은 hero로 묶는다.
        Update_PreviewHeroVisibility();
        Apply_PreviewOverrides();
    }

    Refresh_CharacterSelectUI(); // 현재 선택 hero stain을 여기서 같이 맞춘다.

    const Shared<Player_WorldBase> hero = CLIENT->Find_Hero(heroId);
    const Shared<PlayerStatCom> statCom = hero ? hero->Get_Component<PlayerStatCom>() : nullptr;
    Refresh_CharacterInfo(statCom); // 우측 캐릭터 패널은 무기 데이터 유무와 무관하게 항상 갱신한다.

    const string characterId = Get_CharacterId(heroId);
    const vector<GameDataTable::WeaponDesc>* weapons = CLIENT->Get_WeaponDescs(characterId);
    if (weapons == nullptr || weapons->empty())
    {
        LOG_WARN("ChangeWeaponPanel weapon data not found: {}", characterId);

        // 무기 데이터가 없으면 이전 hero row가 남지 않게 전부 닫는다.
        for (WeaponRowUI& row : _weaponRows)
        {
            if (row.button)
                row.button->Close();
        }

        return;
    }

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

    Refresh_Tooltip((*weapons)[_selectedWeaponIndex]);
    Refresh_TooltipDetails((*weapons)[_selectedWeaponIndex]);
    Refresh_WeaponRows(*weapons);
}

void ChangeWeapon_Panel::Refresh_Tooltip(const GameDataTable::WeaponDesc& weapon)
{
    Apply_WeaponSummary(_tooltipText, weapon);
}

void ChangeWeapon_Panel::Refresh_TooltipDetails(const GameDataTable::WeaponDesc& weapon)
{
    UNREFERENCED_PARAMETER(weapon);

    // 음.. 이거 코드로 바꿔야하나

}

void ChangeWeapon_Panel::Show_WeaponTooltip(bool isVisible)
{
    if (_weaponTooltipRoot == nullptr)
        return;

    if (isVisible)
        _weaponTooltipRoot->Open();
    else
        _weaponTooltipRoot->Close();
}

void ChangeWeapon_Panel::Refresh_WeaponRows(const vector<GameDataTable::WeaponDesc>& weapons)
{
    for (size_t i = 0; i < WeaponSlotCount; ++i)
    {
        WeaponRowUI& row = _weaponRows[i];
        if (row.button == nullptr)
            continue;

        if (i >= weapons.size())
        {
            row.button->Close(); // 실제 무기가 없는 row는 버튼 루트째 닫아서 하위 UI를 같이 숨긴다.
            continue;
        }

        row.button->Open(); // 현재 hero가 가진 row만 다시 연다.

        if (row.icon && !weapons[i].portraitTexturePath.empty())
            row.icon->Set_TexturePath(weapons[i].portraitTexturePath); // DT_Weapon portrait를 row 아이콘 source of truth로 쓴다.

        const wstring backgroundTexturePath = Resolve_WeaponBackgroundTexturePath(weapons[i].weaponId);
        if (row.background && !backgroundTexturePath.empty())
            row.background->Set_TexturePath(backgroundTexturePath);

        if (row.weaponNameText)
            row.weaponNameText->Set_Text(weapons[i].weaponName);

        if (row.levelText)
            row.levelText->Set_Text(to_wstring(weapons[i].level));
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

wstring ChangeWeapon_Panel::Get_DisplayHeroName(HeroId heroId)
{
    switch (heroId)
    {
    case HeroId::Gustave:
        return L"구스타브";
    case HeroId::Lune:
        return L"루네";
    case HeroId::Maelle:
        return L"마엘";
    case HeroId::END:
    default:
        return L"-";
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
        previewPostProcess.bloomThreshold = 0.78f;
        previewPostProcess.bloomIntensity = 0.64f;
        previewPostProcess.bloomScatter = 0.88f;
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

    _tooltipText = {};
    _characterButtons = {};
    _characterStains = {};
    _weaponRows = {};
    _characterNameText.reset();
    _currentLevelText.reset();
    _currentExpText.reset();
    _maxExpText.reset();
    _currentHpText.reset();
    _maxHpText.reset();
    _currentStatHpText.reset();
    _currentStatAttackText.reset();
    _currentStatDefenseText.reset();
    _currentStatSpeedText.reset();
    _currentStatCriticalPercentText.reset();
    _expGaugeImage.reset();
    _hpGaugeImage.reset();
    _weaponTooltipRoot.reset();

    _root.reset();
    _uiByName.clear();

    UI_RuntimePanelBase::Free();
}
