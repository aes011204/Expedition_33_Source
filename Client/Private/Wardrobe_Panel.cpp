#include "Wardrobe_Panel.h"

#include "Client_Constants.h"
#include "ClientInstance.h"
#include "CostumeManager.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "LightingPipeline.h"
#include "Player_WorldBase.h"
#include "PostProcessPipeline.h"
#include "PreviewRimFeature.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr std::array<HeroId, 3> kHeroOrder =
    {
        HeroId::Lune,
        HeroId::Gustave,
        HeroId::Maelle,
    };

    constexpr std::array<const char*, 3> kWardrobeSlotNames =
    {
        "First",
        "Second",
        "Third",
    };

    constexpr std::array<const char*, 3> kWardrobePresetIds =
    {
        "Default",   // First 카드
        "Baguette",  // Second 카드
        nullptr,     // Third 카드 = 슬픔, 아직 미구현
    };

    constexpr Vec3 kPreviewLightDirection = Vec3(-0.42f, -0.78f, 0.46f);
    constexpr Vec4 kPreviewLightDiffuse = Vec4(0.92f, 0.89f, 0.84f, 1.f);
    constexpr Vec4 kPreviewLightAmbient = Vec4(0.16f, 0.16f, 0.18f, 1.f);
    constexpr Vec4 kPreviewLightSpecular = Vec4(0.75f, 0.72f, 0.68f, 1.f);
    constexpr float kPreviewShadowIntensity = 0.58f;

    constexpr Vec3 kPreviewRimColor = Vec3(0.82f, 0.81f, 0.78f);
    constexpr float kPreviewRimIntensity = 0.24f;
    constexpr float kPreviewRimPower = 2.7f;
    constexpr float kPreviewHairRimIntensity = 0.11f;

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

    vector<UIPreviewManager::PreviewSlotDesc> Build_WardrobePreviewLayout()
    {
        return
        {
            { HeroId::Lune,    Vec3(1.1f,  0.f, -0.1f), Vec3(0.f, 0.f, -0.1f) },
            { HeroId::Gustave, Vec3(0.0f,  0.f,  0.f), Vec3(0.f, 0.f,  0.f) },
            { HeroId::Maelle,  Vec3(-1.1f, 0.f, -0.1f), Vec3(0.f, 0.f, -0.1f) },
        };
    }

    void Show_WardrobePreview(HeroId heroId)
    {
        const auto previewLayout = Build_WardrobePreviewLayout();
        CLIENT->Apply_PreviewLayout(previewLayout);

        // 아직 안만들었으니까 일단은, 카메라 프리뷰 SKill Tree꺼 재사용
        CLIENT->Show_Preview(heroId, PreviewShotPanel::SkillTree);
    }

    int32 To_HeroIndex(HeroId heroId)
    {
        for (int32 i = 0; i < static_cast<int32>(kHeroOrder.size()); ++i)
        {
            if (kHeroOrder[i] == heroId)
                return i;
        }

        return -1;
    }

    const char* To_HeroDebugName(HeroId heroId)
    {
        switch (heroId)
        {
        case HeroId::Lune:
            return "Lune";
        case HeroId::Gustave:
            return "Gustave";
        case HeroId::Maelle:
            return "Maelle";
        default:
            return "Unknown";
        }
    }
}

HRESULT Wardrobe_Panel::Initialize(uint32 levelIndex)
{
    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately(false);

    return S_OK;
}

void Wardrobe_Panel::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_Open())
        return;

    Handle_SelectionInput();
    Handle_WardrobeInput();
}

void Wardrobe_Panel::Open()
{
    if (_root == nullptr)
        return;

    if (Is_Open())
        return;

    UI_RuntimePanelBase::Open();
    Set_UIMapOpenState(_uiByName, true);

    if (_selectedHeroId == HeroId::END)
        _selectedHeroId = Resolve_DefaultHeroId();

    CLIENT->Begin_Preview(PreviewShotPanel::SkillTree); // Wardrobe도 기존 preview 플레이어를 그대로 사용한다.
    Sync_CurrentCostumeToPreviewHeroes();               // preview가 새로 스폰된 직후 current costume 상태를 먼저 맞춘다.
    Show_WardrobePreview(_selectedHeroId);
    Update_PreviewHeroVisibility();
    Apply_PreviewOverrides();
    Refresh_VisibleWardrobeCards();

    GAME->Play_SoundCue(Constants::SoundCue::UI_Global_GenericEnter);
    GAME->Unlock_Mouse();
    GAME->Set_GameplayPaused(true);
}

void Wardrobe_Panel::Close()
{
    Close_Immediately(true);
}

void Wardrobe_Panel::Close_Immediately(bool restoreWorldControl)
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

void Wardrobe_Panel::Close_ForMenuTransition()
{
    Close_Immediately(false);
}

void Wardrobe_Panel::Set_SelectedHeroId(HeroId heroId)
{
    _selectedHeroId = (heroId == HeroId::END) ? Resolve_DefaultHeroId() : heroId;

    if (Is_Open())
    {
        Sync_CurrentCostumeToPreviewHero(_selectedHeroId); // 선택 hero preview가 항상 현재 장착 상태를 보게 맞춘다.
        Show_WardrobePreview(_selectedHeroId);
        Update_PreviewHeroVisibility();
        Apply_PreviewOverrides();
        Refresh_VisibleWardrobeCards();
    }
}

void Wardrobe_Panel::Handle_SelectionInput()
{
    const vector<HeroPartyEntry>& partyEntries = CLIENT->Get_HeroEntries();
    if (partyEntries.empty())
        return;

    HeroId previousHeroId = (_selectedHeroId == HeroId::END) ? partyEntries.front().heroId : _selectedHeroId;
    HeroId nextHeroId = previousHeroId;

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

        nextHeroId = partyEntries[currentIndex].heroId;
    }

    for (size_t i = 0; i < _characterButtons.size(); ++i)
    {
        if (_characterButtons[i] == nullptr || !_characterButtons[i]->Was_Clicked())
            continue;

        const HeroId clickedHeroId = kHeroOrder[i];
        const bool hasHeroInParty = std::any_of(
            partyEntries.begin(),
            partyEntries.end(),
            [clickedHeroId](const HeroPartyEntry& entry)
            {
                return entry.heroId == clickedHeroId;
            });

        if (hasHeroInParty)
            nextHeroId = clickedHeroId;
    }

    if (nextHeroId == previousHeroId)
        return;

    _selectedHeroId = nextHeroId;
    GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_SelectAlly);

    Sync_CurrentCostumeToPreviewHero(_selectedHeroId); // 선택이 바뀌면 그 hero preview 파츠를 먼저 맞춘다.
    Show_WardrobePreview(_selectedHeroId);
    Update_PreviewHeroVisibility();
    Apply_PreviewOverrides();
    Refresh_VisibleWardrobeCards();
}

void Wardrobe_Panel::Handle_WardrobeInput()
{
    const int32 heroIndex = To_HeroIndex(_selectedHeroId);
    if (heroIndex < 0)
        return;

    HeroWardrobeUI& currentHeroWardrobe = _heroWardrobeUI[heroIndex];
    for (size_t slotIndex = 0; slotIndex < currentHeroWardrobe.buttons.size(); ++slotIndex)
    {
        const Shared<UI_Button>& button = currentHeroWardrobe.buttons[slotIndex];
        if (button == nullptr || !button->Was_Clicked())
            continue;

        GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);

        if (slotIndex == 2)
        {
            LOG_DEBUG("Wardrobe_Panel: clicked sorrow costume card. hero={}", To_HeroDebugName(_selectedHeroId)); // 슬픔은 아직 장착하지 않고 로그만 남긴다.
            return;
        }

        const char* presetId = kWardrobePresetIds[slotIndex];
        if (!Try_EquipPreset(presetId))
        {
            LOG_WARN(
                "Wardrobe_Panel: failed to equip preset. hero={}, slot={}",
                To_HeroDebugName(_selectedHeroId),
                kWardrobeSlotNames[slotIndex]);
        }

        return;
    }
}

void Wardrobe_Panel::Refresh_VisibleWardrobeCards()
{
    const int32 selectedHeroIndex = To_HeroIndex(_selectedHeroId);
    if (selectedHeroIndex < 0)
        return;

    for (int32 heroIndex = 0; heroIndex < static_cast<int32>(_heroWardrobeUI.size()); ++heroIndex)
    {
        HeroWardrobeUI& heroWardrobe = _heroWardrobeUI[heroIndex];
        const bool isSelectedHero = heroIndex == selectedHeroIndex;

        for (Shared<UI_Base>& panel : heroWardrobe.panels)
        {
            if (panel == nullptr)
                continue;

            if (isSelectedHero)
                panel->Open();
            else
                panel->Close();
        }
    }
}

void Wardrobe_Panel::Sync_CurrentCostumeToPreviewHeroes()
{
    const vector<HeroPartyEntry>& partyEntries = CLIENT->Get_HeroEntries();
    for (const HeroPartyEntry& entry : partyEntries)
        Sync_CurrentCostumeToPreviewHero(entry.heroId);
}

void Wardrobe_Panel::Sync_CurrentCostumeToPreviewHero(HeroId heroId)
{
    CostumeManager* costumeManager = CLIENT->Get_CostumeManager();
    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    if (costumeManager == nullptr || previewManager == nullptr)
        return;

    const CostumePreset* preset = costumeManager->Get_CurrentCostume(heroId);
    if (preset == nullptr)
        return;

    const auto& previewHeroes = previewManager->Get_PreviewHeroes();
    const int32 previewHeroIndex = static_cast<int32>(heroId);
    if (previewHeroIndex < 0 || previewHeroIndex >= static_cast<int32>(previewHeroes.size()))
        return;

    const UIPreviewManager::PreviewHeroEntry& previewHero = previewHeroes[previewHeroIndex];
    if (previewHero.player == nullptr)
        return;

    if (FAILED(previewHero.player->Apply_CostumePreset(*preset)))
    {
        LOG_WARN(
            "Wardrobe_Panel: failed to sync preview costume. hero={}, preset={}",
            To_HeroDebugName(heroId),
            preset->presetId);
    }
}

bool Wardrobe_Panel::Try_EquipPreset(const char* presetId)
{
    if (presetId == nullptr || presetId[0] == '\0')
        return false;

    CostumeManager* costumeManager = CLIENT->Get_CostumeManager();
    if (costumeManager == nullptr)
        return false;

    // CostumeManager current preset 갱신
    const CostumePreset* preset = costumeManager->Set_CurrentCostume(_selectedHeroId, presetId);
    if (preset == nullptr)
        return false;

    const Shared<Player_WorldBase> worldHero = CLIENT->Find_Hero(_selectedHeroId);
    if (worldHero == nullptr)
        return false;

    if (FAILED(worldHero->Apply_CostumePreset(*preset))) // 월드 hero 파츠 재구성
        return false;

    Sync_CurrentCostumeToPreviewHero(_selectedHeroId);   // 같은 preset을 preview hero에도 즉시 반영
    Show_WardrobePreview(_selectedHeroId);
    Update_PreviewHeroVisibility();
    Apply_PreviewOverrides();

    return true;
}

void Wardrobe_Panel::Apply_PreviewOverrides()
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

void Wardrobe_Panel::Restore_PreviewOverrides()
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

void Wardrobe_Panel::Apply_PreviewRimState()
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

void Wardrobe_Panel::Clear_PreviewRimState()
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

void Wardrobe_Panel::Update_PreviewHeroVisibility()
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

HeroId Wardrobe_Panel::Resolve_DefaultHeroId()
{
    const vector<HeroPartyEntry>& partyEntries = CLIENT->Get_HeroEntries();
    if (!partyEntries.empty())
        return partyEntries.front().heroId;

    return HeroId::Gustave;
}

HRESULT Wardrobe_Panel::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Wardrobe_Path;
    desc.prototypeTagPrefix = L"Wardrobe";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_Wardrobe");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT Wardrobe_Panel::Bind_UI()
{
    _characterButtons[0] = Find_TypedUI<UI_Button>(_uiByName, L"Button_CharacterSelect_01");
    _characterButtons[1] = Find_TypedUI<UI_Button>(_uiByName, L"Button_CharacterSelect_02");
    _characterButtons[2] = Find_TypedUI<UI_Button>(_uiByName, L"Button_CharacterSelect_03");

    _heroWardrobeUI[0].panels[0] = Find_UI(_uiByName, L"Panel_Wardrobe_Lune_First");
    _heroWardrobeUI[0].panels[1] = Find_UI(_uiByName, L"Panel_Wardrobe_Lune_Second");
    _heroWardrobeUI[0].panels[2] = Find_UI(_uiByName, L"Panel_Wardrobe_Lune_Third");
    _heroWardrobeUI[0].buttons[0] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Wardrobe_Lune_First");
    _heroWardrobeUI[0].buttons[1] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Wardrobe_Lune_Second");
    _heroWardrobeUI[0].buttons[2] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Wardrobe_Lune_Third");

    _heroWardrobeUI[1].panels[0] = Find_UI(_uiByName, L"Panel_Wardrobe_Gustave_First");
    _heroWardrobeUI[1].panels[1] = Find_UI(_uiByName, L"Panel_Wardrobe_Gustave_Second");
    _heroWardrobeUI[1].panels[2] = Find_UI(_uiByName, L"Panel_Wardrobe_Gustave_Third");
    _heroWardrobeUI[1].buttons[0] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Wardrobe_Gustave_First");
    _heroWardrobeUI[1].buttons[1] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Wardrobe_Gustave_Second");
    _heroWardrobeUI[1].buttons[2] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Wardrobe_Gustave_Third");

    _heroWardrobeUI[2].panels[0] = Find_UI(_uiByName, L"Panel_Wardrobe_Maelle_First");
    _heroWardrobeUI[2].panels[1] = Find_UI(_uiByName, L"Panel_Wardrobe_Maelle_Second");
    _heroWardrobeUI[2].panels[2] = Find_UI(_uiByName, L"Panel_Wardrobe_Maelle_Third");
    _heroWardrobeUI[2].buttons[0] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Wardrobe_Maelle_First");
    _heroWardrobeUI[2].buttons[1] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Wardrobe_Maelle_Second");
    _heroWardrobeUI[2].buttons[2] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Wardrobe_Maelle_Third");

    CHECK_NULL(_characterButtons[0], E_FAIL);
    CHECK_NULL(_characterButtons[1], E_FAIL);
    CHECK_NULL(_characterButtons[2], E_FAIL);

    for (const HeroWardrobeUI& heroWardrobe : _heroWardrobeUI)
    {
        for (const Shared<UI_Base>& panel : heroWardrobe.panels)
            CHECK_NULL(panel, E_FAIL);

        for (const Shared<UI_Button>& button : heroWardrobe.buttons)
            CHECK_NULL(button, E_FAIL);
    }

    return S_OK;
}

Unique<Wardrobe_Panel> Wardrobe_Panel::Create(uint32 levelIndex)
{
    auto instance = make_unique<Wardrobe_Panel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : Wardrobe_Panel");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void Wardrobe_Panel::Free()
{
    if (Is_Open())
        Close_Immediately(false);

    for (Shared<UI_Button>& button : _characterButtons)
        button.reset();

    for (HeroWardrobeUI& heroWardrobe : _heroWardrobeUI)
    {
        for (Shared<UI_Base>& panel : heroWardrobe.panels)
            panel.reset();

        for (Shared<UI_Button>& button : heroWardrobe.buttons)
            button.reset();
    }

    _root.reset();
    _uiByName.clear();

    UI_RuntimePanelBase::Free();
}
