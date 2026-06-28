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

    constexpr Vec4 kWeaponFrameDefaultTint{ 1.f, 1.f, 1.f, 1.f };
    constexpr Vec4 kWeaponFrameEquippedTint{ 0.78f, 0.36f, 0.58f, 1.f };
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

    if (_selectedHeroId == HeroId::END)
        _selectedHeroId = HeroId::Lune;

    CLIENT->Begin_Preview(PreviewShotPanel::ChangeWeapon);
    Show_ChangeWeaponPreview(_selectedHeroId);
	CLIENT->Play_PreviewAnimation(_selectedHeroId, PreviewAnimState::Weapon_Intro, PreviewAnimState::Weapon_Idle);

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
    _isProgressionContentMode = false;
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
    Refresh_ForSelectedHero(_selectedHeroId);
}

void ChangeWeapon_Panel::Close_AsProgressionContent()
{
    if (!_isProgressionContentMode && !Is_Open())
        return;

    _isProgressionContentMode = false;
    Show_WeaponTooltip(false);
    UI_RuntimePanelBase::Close();
    Set_UIMapOpenState(_uiByName, false);
}

void ChangeWeapon_Panel::Refresh_ForSelectedHero(HeroId selectedHeroId)
{
    _selectedHeroId = (selectedHeroId == HeroId::END) ? HeroId::Lune : selectedHeroId;
    Refresh_WeaponUI();
    Show_WeaponTooltip(false);
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

    return S_OK;
}

HRESULT ChangeWeapon_Panel::Bind_UI()
{
    _tooltipText.weaponNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_WeaponName");
    _tooltipText.levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentWeaponLevel");
    _tooltipText.powerText = Find_TypedUI<UI_Text>(_uiByName, L"Text_WeaponTooltip_PowerValue");
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

    CHECK_NULL(_tooltipText.weaponNameText, E_FAIL);
    CHECK_NULL(_tooltipText.levelText, E_FAIL);
    CHECK_NULL(_tooltipText.powerText, E_FAIL);
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
        if (statCom)
        {
            statCom->Set_EquippedWeapon(
                selectedWeapon.weaponId,
                CLIENT->Resolve_WeaponPower(characterId, selectedWeapon.weaponId));
        }

        CLIENT->Sync_PreviewHeroRuntimeData(heroId);
        Apply_ChangeWeaponPreviewWeapon(heroId);
        Refresh_WeaponUI(); // 장착 row 보라색 표시와 selected index를 새 장착값 기준으로 다시 맞춘다.

        Refresh_Tooltip(selectedWeapon);
        Refresh_TooltipDetails(selectedWeapon); // 설명/등급/능력치도 같이 갱신
        Show_WeaponTooltip(true);

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
            if (row.button)
                row.button->Close();
        }

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

    Refresh_Tooltip((*weapons)[_selectedWeaponIndex]);
    Refresh_TooltipDetails((*weapons)[_selectedWeaponIndex]);
    Refresh_WeaponRows(*weapons, equippedWeaponId);
}

void ChangeWeapon_Panel::Refresh_Tooltip(const GameDataTable::WeaponDesc& weapon)
{
    const string characterId = Get_CharacterId((_selectedHeroId == HeroId::END) ? HeroId::Lune : _selectedHeroId);
    Apply_WeaponSummary(
        _tooltipText,
        weapon,
        CLIENT->Resolve_WeaponEnhanceLevel(characterId, weapon.weaponId),
        CLIENT->Resolve_WeaponPower(characterId, weapon.weaponId));
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
            row.button->Close();
            continue;
        }

        const GameDataTable::WeaponDesc& weapon = weapons[i];
        const bool isOwned = inventory == nullptr || inventory->Is_WeaponUnlocked(characterId, weapon.weaponId);
        if (!isOwned)
        {
            row.button->Close(); // 아직 소유하지 않은 무기는 ChangeWeapon 선택 목록에서 숨긴다.
            continue;
        }

        row.button->Open();

        if (row.icon && !weapon.portraitTexturePath.empty())
            row.icon->Set_TexturePath(weapon.portraitTexturePath);

        const wstring backgroundTexturePath = Resolve_WeaponBackgroundTexturePath(weapon.weaponId);
        if (row.background)
        {
            if (!backgroundTexturePath.empty())
                row.background->Set_TexturePath(backgroundTexturePath);

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

    _tooltipText = {};
    _isProgressionContentMode = false;
    _weaponRows = {};
    _weaponTooltipRoot.reset();

    _root.reset();
    _uiByName.clear();

    UI_RuntimePanelBase::Free();
}
