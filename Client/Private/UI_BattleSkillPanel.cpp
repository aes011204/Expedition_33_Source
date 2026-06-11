#include "UI_BattleSkillPanel.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Animation.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t UI_Battle_GustaveSkillPanel_Path[] = L"UI/UI_Json/BattleSystem/Gustave/UI_BattleSystem_Skill_Gustave.json";
    constexpr wchar_t UI_Battle_LuneSkillPanel_Path[] = L"UI/UI_Json/BattleSystem/Lune/UI_BattleSystem_Skill_Lune.json";
    constexpr wchar_t UI_Battle_MaelleSkillPanel_Path[] = L"UI/UI_Json/BattleSystem/Maelle/UI_BattleSystem_Skill_Maelle.json";

    constexpr wchar_t UI_BattleSkillTooltip_LumiereAssault_Path[] = L"UI/UI_Json/BattleSystem/SkillTooltip/Gustave/UI_BattleSystem_SkillTooltip_LumiereAssault.json";
    constexpr wchar_t UI_BattleSkillTooltip_UnleashedCharge_Path[] = L"UI/UI_Json/BattleSystem/SkillTooltip/Gustave/UI_BattleSystem_SkillTooltip_UnleashedCharge.json";
    constexpr wchar_t UI_BattleSkillTooltip_MarkingShot_Path[] = L"UI/UI_Json/BattleSystem/SkillTooltip/Gustave/UI_BattleSystem_SkillTooltip_MarkingShot.json";

    constexpr wchar_t UI_BattleSkillTooltip_Immolation_Path[] = L"UI/UI_Json/BattleSystem/SkillTooltip/Lune/UI_BattleSystem_SkillTooltip_Immolation.json";
    constexpr wchar_t UI_BattleSkillTooltip_IceGust_Path[] = L"UI/UI_Json/BattleSystem/SkillTooltip/Lune/UI_BattleSystem_SkillTooltip_IceGust.json";
    constexpr wchar_t UI_BattleSkillTooltip_Thunderfall_Path[] = L"UI/UI_Json/BattleSystem/SkillTooltip/Lune/UI_BattleSystem_SkillTooltip_Thunderfall.json";

    constexpr wchar_t UI_BattleSkillTooltip_OffensiveSwitch_Path[] = L"UI/UI_Json/BattleSystem/SkillTooltip/Maelle/UI_BattleSystem_SkillTooltip_OffensiveSwitch.json";
    constexpr wchar_t UI_BattleSkillTooltip_Percee_Path[] = L"UI/UI_Json/BattleSystem/SkillTooltip/Maelle/UI_BattleSystem_SkillTooltip_Percee.json";
    constexpr wchar_t UI_BattleSkillTooltip_FireRise_Path[] = L"UI/UI_Json/BattleSystem/SkillTooltip/Maelle/UI_BattleSystem_SkillTooltip_FireRise.json";

    constexpr wchar_t UI_BattleSkillTooltip_FleureFury_Path[] =
        L"UI/UI_Json/BattleSystem/SkillTooltip/Maelle/UI_BattleSystem_SkillTooltip_FleuretFury.json";

    constexpr wchar_t kTooltipIntroAnimationName[] = L"BattleSystem_Tooltip_Intro";
    constexpr wchar_t kTooltipOutroAnimationName[] = L"BattleSystem_Tooltip_Outro";

    constexpr wchar_t kBattleSkillGustaveIntroAnimationName[] = L"BattleSystem_Skill_Gustave_Intro";
    constexpr wchar_t kBattleSkillGustaveLoopAnimationName[] = L"BattleSystem_Skill_Gustave_Loop";
    constexpr wchar_t kBattleSkillLuneIntroAnimationName[] = L"BattleSystem_Skill_Lune_Intro";
    constexpr wchar_t kBattleSkillLuneLoopAnimationName[] = L"BattleSystem_Skill_Lune_Loop";
    constexpr wchar_t kBattleSkillMaelleIntroAnimationName[] = L"BattleSystem_Skill_Maelle_Intro";
    constexpr wchar_t kBattleSkillMaelleLoopAnimationName[] = L"BattleSystem_Skill_Maelle_Loop";

    float Resolve_BattleSkillIntroDurationSec()
    {
        UI_KeyframeAnimationAsset introAnimation{};
        introAnimation.fps = 40;
        introAnimation.startFrame = 0;
        introAnimation.endFrame = 60;
        return Get_KeyframeAnimationDurationSec(introAnimation);
    }
}

HRESULT UI_BattleSkillPanel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);
    CHECK_FAILED(Load_TooltipAnimations(), E_FAIL);

    _mainPanelIntroDuration = Resolve_BattleSkillIntroDurationSec();

    Close_Immediately();

    return S_OK;
}

void UI_BattleSkillPanel::Update(float timeDelta)
{
    if (_activeView == ActiveView::MainPanel && _isMainPanelOpen && _isPlayingMainPanelIntro)
    {
        _mainPanelIntroElapsed += max(0.f, timeDelta);

        if (_mainPanelIntroElapsed >= _mainPanelIntroDuration)
        {
            _isPlayingMainPanelIntro = false;

            switch (_activeHeroId)
            {
            case HeroId::Gustave:
                Play_MainPanelAnimation(_activeHeroId, kBattleSkillGustaveLoopAnimationName);
                break;

            case HeroId::Lune:
                Play_MainPanelAnimation(_activeHeroId, kBattleSkillLuneLoopAnimationName);
                break;

            case HeroId::Maelle:
                Play_MainPanelAnimation(_activeHeroId, kBattleSkillMaelleLoopAnimationName);
                break;

            case HeroId::END:
            default:
                break;
            }
        }
    }

    if (_activeView != ActiveView::Tooltip || !_isTooltipClosingWithOutro)
        return;

    _tooltipCloseElapsed += max(0.f, timeDelta);

    if (_tooltipCloseElapsed >= _tooltipOutroDuration)
        Close_TooltipImmediately();
}

void UI_BattleSkillPanel::Open_MainPanel(PlayerCharacterType playerType)
{
    Shared<UI_Base> nextRoot = nullptr;

    switch (playerType)
    {
    case PlayerCharacterType::Gustave:
        nextRoot = _gustavePanel;
        break;

    case PlayerCharacterType::Lune:
        nextRoot = _lunePanel;
        break;

    case PlayerCharacterType::Maelle:
        nextRoot = _maellePanel;
        break;

    default:
        break;
    }

    _activeHeroId = Resolve_HeroId(playerType);

    if (_activeView == ActiveView::MainPanel && _root == nextRoot && _isOpen)
    {
        Apply_CurrentRootEditorOffsets();
        return;
    }

    if (_activeView == ActiveView::Tooltip)
    {
        Close();
        return;
    }

    Mark_EditorRuntimeTuningDirty();

    Close_MainPanels();

    _root = nextRoot;
    _activeView = (_root != nullptr) ? ActiveView::MainPanel : ActiveView::None;
    Reset_EditorDesignPositionCache();

    if (_root == nullptr)
    {
        _isOpen = false;
        _activeHeroId = HeroId::END;
        return;
    }

    UI_RuntimePanelBase::Open();

    Open_MainPanelUIs(_activeHeroId);
    Apply_CurrentRootEditorOffsets();
    _isMainPanelOpen = true;
    _isPlayingMainPanelIntro = true;
    _mainPanelIntroElapsed = 0.f;
    Set_MainPanelSkillOptionsAlpha(_activeHeroId, 0.f);

    switch (_activeHeroId)
    {
    case HeroId::Gustave:
        if (!Play_MainPanelAnimation(_activeHeroId, kBattleSkillGustaveIntroAnimationName))
        {
            _isPlayingMainPanelIntro = false;
            Play_MainPanelAnimation(_activeHeroId, kBattleSkillGustaveLoopAnimationName);
        }
        break;

    case HeroId::Lune:
        if (!Play_MainPanelAnimation(_activeHeroId, kBattleSkillLuneIntroAnimationName))
        {
            _isPlayingMainPanelIntro = false;
            Play_MainPanelAnimation(_activeHeroId, kBattleSkillLuneLoopAnimationName);
        }
        break;

    case HeroId::Maelle:
        if (!Play_MainPanelAnimation(_activeHeroId, kBattleSkillMaelleIntroAnimationName))
        {
            _isPlayingMainPanelIntro = false;
            Play_MainPanelAnimation(_activeHeroId, kBattleSkillMaelleLoopAnimationName);
        }
        break;

    case HeroId::END:
    default:
        _isPlayingMainPanelIntro = false;
        break;
    }
}

void UI_BattleSkillPanel::Open_Tooltip(const string& skillKey)
{
    auto iter = _tooltipRoots.find(skillKey);
    if (iter == _tooltipRoots.end() || iter->second == nullptr)
    {
        Close_Immediately();
        return;
    }

    Shared<UI_Base> nextRoot = iter->second;

    // 같은 tooltip이 이미 정상 open 상태면 intro를 다시 재생하지 않는다.
    if (_activeView == ActiveView::Tooltip &&
        _root == nextRoot &&
        _isOpen &&
        !_isTooltipClosingWithOutro)
    {
        return;
    }

    Mark_EditorRuntimeTuningDirty();

    Close_MainPanels();
    Close_TooltipImmediately();

    _activeHeroId = HeroId::END;
    _activeTooltipRoot = nextRoot;
    _root = nextRoot;
    _activeView = ActiveView::Tooltip;
    Reset_EditorDesignPositionCache();

    UI_RuntimePanelBase::Open();

    if (_hasTooltipIntroAnimation)
        _root->Play_UIAnimation(kTooltipIntroAnimationName, true);
}

void UI_BattleSkillPanel::Close()
{
    if (_activeView == ActiveView::Tooltip)
    {
        if (_root == nullptr)
        {
            Close_TooltipImmediately();
            return;
        }

        // outro 중 중복 Close 방지
        if (_isTooltipClosingWithOutro)
            return;

        // outro asset이 없으면 기존처럼 즉시 닫는다.
        if (!_hasTooltipOutroAnimation)
        {
            Close_TooltipImmediately();
            return;
        }

        _isTooltipClosingWithOutro = true;
        _tooltipCloseElapsed = 0.f;

        // SkillTooltip close 시 공용 outro 재생
        if (!_root->Play_UIAnimation(kTooltipOutroAnimationName, true))
            Close_TooltipImmediately();

        return;
    }

    // 수정: 메인 패널은 기존처럼 즉시 close
    Close_Immediately();
}

void UI_BattleSkillPanel::Close_Immediately()
{
    const bool wasOpen = _activeView != ActiveView::None || _isOpen;

    Close_MainPanels();
    Close_Tooltips();

    _root = nullptr;
    _activeTooltipRoot.reset();
    _activeHeroId = HeroId::END;
    _activeView = ActiveView::None;
    _isTooltipClosingWithOutro = false;
    _tooltipCloseElapsed = 0.f;

    Reset_EditorDesignPositionCache();

    _isOpen = false;

    if (wasOpen)
        Mark_EditorRuntimeTuningDirty();
}

void UI_BattleSkillPanel::Set_EditorDefaultTestOffset(const Vec2& offset)
{
    UI_RuntimePanelBase::Set_EditorDefaultTestOffset(offset);

    if (_isOpen && _activeHeroId != HeroId::END)
        Apply_CurrentRootEditorOffsets();
}

Vec2 UI_BattleSkillPanel::Get_EditorHeroTestOffset(HeroId heroId) const
{
    return Resolve_HeroTestOffset(heroId);
}

void UI_BattleSkillPanel::Set_EditorHeroTestOffset(HeroId heroId, const Vec2& offset)
{
    switch (heroId)
    {
    case HeroId::Gustave:
        _gustaveTestOffset = offset;
        break;

    case HeroId::Lune:
        _luneTestOffset = offset;
        break;

    case HeroId::Maelle:
        _maelleTestOffset = offset;
        break;

    case HeroId::END:
    default:
        break;
    }

    if (_isOpen && _activeHeroId == heroId)
        Apply_CurrentRootEditorOffsets();

    Mark_EditorRuntimeTuningDirty();
}

HRESULT UI_BattleSkillPanel::Load_UI(uint32 levelIndex)
{
    _uiByName.clear();
    _gustaveMainPanelUIs.clear();
    _luneMainPanelUIs.clear();
    _maelleMainPanelUIs.clear();

    auto Bind_MainPanelAnimTarget = [](const umap<wstring, Shared<UI_Base>>& loadedUIByName, const wchar_t* name, vector<Shared<UI_Base>>& outTargets) -> HRESULT
        {
            Shared<UI_Base> ui = Find_UI(loadedUIByName, name);
            CHECK_NULL(ui, E_FAIL);
            outTargets.push_back(ui);
            return S_OK;
        };

    auto Load_AndMerge = [&](
        const wchar_t* jsonPath,
        const wchar_t* prototypeTagPrefix,
        const vector<wstring>& animationPaths,
        vector<Shared<UI_Base>>* mainPanelTargets,
        const vector<const wchar_t*>& targetNames) -> HRESULT
        {
            UI_RuntimeLoader::UI_LoadDesc desc{};
            desc.levelIndex = levelIndex;
            desc.jsonPath = jsonPath;
            desc.animationPaths = animationPaths;
            desc.prototypeTagPrefix = prototypeTagPrefix;

            umap<wstring, Shared<UI_Base>> loadedUIByName{};
            desc.outUIByName = &loadedUIByName;

            CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

            if (mainPanelTargets != nullptr)
            {
                for (const wchar_t* targetName : targetNames)
                    CHECK_FAILED(Bind_MainPanelAnimTarget(loadedUIByName, targetName, *mainPanelTargets), E_FAIL);
            }

            for (auto& [name, ui] : loadedUIByName)
                _uiByName[name] = ui;

            return S_OK;
        };

    CHECK_FAILED(Load_AndMerge(
        UI_Battle_GustaveSkillPanel_Path,
        L"BattleSkillPanelGustave",
        {
            Constants::UI_Anim::UI_BattleSkillGustaveIntroAnim_Path,
            Constants::UI_Anim::UI_BattleSkillGustaveLoopAnim_Path,
        },
        &_gustaveMainPanelUIs,
        {
            L"Panel_BattleSystem_Skill_Gustave",
            L"Image_BattleSystem_Skill_Gustave_Deco_1",
            L"Image_BattleSystem_Skill_Gustave_Deco_2",
            L"Panel_LumiereAssault",
            L"Panel_UnleashedCharge",
            L"Panel_MarkingShot",
            L"Image_BattleSystem_Skill_Gustave_Deco_3",
        }), E_FAIL);

    CHECK_FAILED(Load_AndMerge(
        UI_Battle_LuneSkillPanel_Path,
        L"BattleSkillPanelLune",
        {
            Constants::UI_Anim::UI_BattleSkillLuneIntroAnim_Path,
            Constants::UI_Anim::UI_BattleSkillLuneLoopAnim_Path,
        },
        &_luneMainPanelUIs,
        {
            L"Panel_BattleSystem_Skill_Lune",
            L"Image_BattleSystem_Skill_Lune_Deco_1",
            L"Image_BattleSystem_Skill_Lune_Deco_2",
            L"Panel_Immolation",
            L"Panel_IceGust",
            L"Panel_Thunderfall",
            L"Image_BattleSystem_Skill_Lune_Deco_3",
        }), E_FAIL);

    CHECK_FAILED(Load_AndMerge(
        UI_Battle_MaelleSkillPanel_Path,
        L"BattleSkillPanelMaelle",
        {
            Constants::UI_Anim::UI_BattleSkillMaelleIntroAnim_Path,
            Constants::UI_Anim::UI_BattleSkillMaelleLoopAnim_Path,
        },
        &_maelleMainPanelUIs,
        {
            L"Panel_BattleSystem_Skill_Maelle",
            L"Image_BattleSystem_Skill_Maelle_Deco_1",
            L"Image_BattleSystem_Skill_Maelle_Deco_2",
            L"Panel_OffensiveSwitch",
            L"Panel_Percee",
            L"Panel_FleuretFury",
            L"Image_BattleSystem_Skill_Maelle_Deco_3",
        }), E_FAIL);

    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_LumiereAssault_Path, L"BattleSkillTooltipLumiereAssault", {}, nullptr, {}), E_FAIL);
    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_UnleashedCharge_Path, L"BattleSkillTooltipUnleashedCharge", {}, nullptr, {}), E_FAIL);
    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_MarkingShot_Path, L"BattleSkillTooltipMarkingShot", {}, nullptr, {}), E_FAIL);

    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_Immolation_Path, L"BattleSkillTooltipImmolation", {}, nullptr, {}), E_FAIL);
    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_IceGust_Path, L"BattleSkillTooltipIceGust", {}, nullptr, {}), E_FAIL);
    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_Thunderfall_Path, L"BattleSkillTooltipThunderfall", {}, nullptr, {}), E_FAIL);

    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_OffensiveSwitch_Path, L"BattleSkillTooltipOffensiveSwitch", {}, nullptr, {}), E_FAIL);
    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_Percee_Path, L"BattleSkillTooltipPercee", {}, nullptr, {}), E_FAIL);
    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_FleureFury_Path, L"BattleSkillTooltipFleureFury", {}, nullptr, {}), E_FAIL);
    //CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_FireRise_Path, L"BattleSkillTooltipFireRise"), E_FAIL);


    return S_OK;
}

HRESULT UI_BattleSkillPanel::Bind_UI()
{
    _gustavePanel = Find_UI(_uiByName, L"Panel_BattleSystem_Skill_Gustave");
    _lunePanel = Find_UI(_uiByName, L"Panel_BattleSystem_Skill_Lune");
    _maellePanel = Find_UI(_uiByName, L"Panel_BattleSystem_Skill_Maelle");

    CHECK_NULL(_gustavePanel, E_FAIL);
    CHECK_NULL(_lunePanel, E_FAIL);
    CHECK_NULL(_maellePanel, E_FAIL);

    auto Bind_Tooltip = [&](const string& skillKey, const wchar_t* rootName) -> HRESULT
        {
            Shared<UI_Base> root = Find_UI(_uiByName, rootName);
            CHECK_NULL(root, E_FAIL);

            _tooltipRoots[skillKey] = root;
            return S_OK;
        };

    CHECK_FAILED(Bind_Tooltip("LumiereAssault", L"Panel_BattleSystem_SkillTooltip_LumiereAssault"), E_FAIL);
    CHECK_FAILED(Bind_Tooltip("UnleashedCharge", L"Panel_BattleSystem_SkillTooltip_UnleashedCharge"), E_FAIL);
    CHECK_FAILED(Bind_Tooltip("MarkingShot", L"Panel_BattleSystem_SkillTooltip_MarkingShot"), E_FAIL);

    CHECK_FAILED(Bind_Tooltip("Immolation", L"Panel_BattleSystem_SkillTooltip_Immolation"), E_FAIL);
    CHECK_FAILED(Bind_Tooltip("IceGust", L"Panel_BattleSystem_SkillTooltip_IceGust"), E_FAIL);
    CHECK_FAILED(Bind_Tooltip("Thunderfall", L"Panel_BattleSystem_SkillTooltip_Thunderfall"), E_FAIL);

    CHECK_FAILED(Bind_Tooltip("OffensiveSwitch", L"Panel_BattleSystem_SkillTooltip_OffensiveSwitch"), E_FAIL);
    CHECK_FAILED(Bind_Tooltip("Percee", L"Panel_BattleSystem_SkillTooltip_Percee"), E_FAIL);
    CHECK_FAILED(Bind_Tooltip("FleureFury", L"Panel_BattleSystem_SkillTooltip_FleuretFury"), E_FAIL);


    //CHECK_FAILED(Bind_Tooltip("FireRise", L"Panel_BattleSystem_SkillTooltip_FireRise"), E_FAIL);

    return S_OK;
}

HRESULT UI_BattleSkillPanel::Load_TooltipAnimations()
{
    _hasTooltipIntroAnimation =
        UI_RuntimeLoader::Load_PublicAnimationAsset(
            Constants::UI_Anim::UI_BattleTooltipIntroAnim_Path,
            _tooltipIntroAnimation);
    CHECK_TRUE(_hasTooltipIntroAnimation, E_FAIL);

    _hasTooltipOutroAnimation =
        UI_RuntimeLoader::Load_PublicAnimationAsset(
            Constants::UI_Anim::UI_BattleTooltipOutroAnim_Path,
            _tooltipOutroAnimation);
    CHECK_TRUE(_hasTooltipOutroAnimation, E_FAIL);

    _tooltipOutroDuration = Get_KeyframeAnimationDurationSec(_tooltipOutroAnimation);

    for (auto& [skillKey, tooltipRoot] : _tooltipRoots)
    {
        if (tooltipRoot == nullptr)
            continue;

        tooltipRoot->Add_UIAnimationAsset(_tooltipIntroAnimation);
        tooltipRoot->Add_UIAnimationAsset(_tooltipOutroAnimation);
    }

    return S_OK;
}

void UI_BattleSkillPanel::Close_MainPanels()
{
    _isMainPanelOpen = false;
    _isPlayingMainPanelIntro = false;
    _mainPanelIntroElapsed = 0.f;

    Close_MainPanelUIs();

    if (_gustavePanel) _gustavePanel->Close();
    if (_lunePanel) _lunePanel->Close();
    if (_maellePanel) _maellePanel->Close();
}

void UI_BattleSkillPanel::Open_MainPanelUIs(HeroId heroId)
{
    vector<Shared<UI_Base>>* targets = Resolve_MainPanelAnimationTargets(heroId);
    if (targets == nullptr)
        return;

    for (const Shared<UI_Base>& ui : *targets)
    {
        if (ui)
            ui->Open();
    }
}

void UI_BattleSkillPanel::Close_MainPanelUIs()
{
    Stop_MainPanelAnimations();

    for (const Shared<UI_Base>& ui : _gustaveMainPanelUIs) if (ui) ui->Close();
    for (const Shared<UI_Base>& ui : _luneMainPanelUIs) if (ui) ui->Close();
    for (const Shared<UI_Base>& ui : _maelleMainPanelUIs) if (ui) ui->Close();
}

void UI_BattleSkillPanel::Stop_MainPanelAnimations()
{
    for (const Shared<UI_Base>& ui : _gustaveMainPanelUIs) if (ui) ui->Stop_UIAnimation();
    for (const Shared<UI_Base>& ui : _luneMainPanelUIs) if (ui) ui->Stop_UIAnimation();
    for (const Shared<UI_Base>& ui : _maelleMainPanelUIs) if (ui) ui->Stop_UIAnimation();
}

void UI_BattleSkillPanel::Set_MainPanelSkillOptionsAlpha(HeroId heroId, float alphaMultiplier)
{
    auto Set_Alpha = [this, alphaMultiplier](const wchar_t* name)
        {
            Shared<UI_Base> ui = Find_UI(_uiByName, name);
            if (ui)
                ui->Set_AlphaMultiplier(alphaMultiplier);
        };

    switch (heroId)
    {
    case HeroId::Gustave:
        Set_Alpha(L"Panel_LumiereAssault");
        Set_Alpha(L"Panel_UnleashedCharge");
        Set_Alpha(L"Panel_MarkingShot");
        break;

    case HeroId::Lune:
        Set_Alpha(L"Panel_Immolation");
        Set_Alpha(L"Panel_IceGust");
        Set_Alpha(L"Panel_Thunderfall");
        break;

    case HeroId::Maelle:
        Set_Alpha(L"Panel_OffensiveSwitch");
        Set_Alpha(L"Panel_Percee");
        Set_Alpha(L"Panel_FleuretFury");
        break;

    case HeroId::END:
    default:
        break;
    }

}

bool UI_BattleSkillPanel::Play_MainPanelAnimation(HeroId heroId, const wchar_t* animationName)
{
    vector<Shared<UI_Base>>* targets = Resolve_MainPanelAnimationTargets(heroId);
    if (targets == nullptr || animationName == nullptr)
        return false;

    bool played = false;

    for (const Shared<UI_Base>& ui : *targets)
    {
        if (ui == nullptr)
            continue;

        const bool playedCurrent = ui->Play_UIAnimation(animationName, true);
        if (playedCurrent)
            ui->Update(0.f);

        played = playedCurrent || played;
    }

    return played;
}

vector<Shared<UI_Base>>* UI_BattleSkillPanel::Resolve_MainPanelAnimationTargets(HeroId heroId)
{
    switch (heroId)
    {
    case HeroId::Gustave:
        return &_gustaveMainPanelUIs;

    case HeroId::Lune:
        return &_luneMainPanelUIs;

    case HeroId::Maelle:
        return &_maelleMainPanelUIs;

    case HeroId::END:
    default:
        return nullptr;
    }
}

void UI_BattleSkillPanel::Close_Tooltips()
{
    for (auto& [skillKey, tooltipRoot] : _tooltipRoots)
    {
        UNREFERENCED_PARAMETER(skillKey);

        if (tooltipRoot == nullptr)
            continue;

        tooltipRoot->Stop_UIAnimation();
        tooltipRoot->Close();
    }
}

void UI_BattleSkillPanel::Close_TooltipImmediately()
{
    Close_Tooltips();

    _root = nullptr;
    _activeTooltipRoot.reset();
    _activeView = ActiveView::None;
    _isTooltipClosingWithOutro = false;
    _tooltipCloseElapsed = 0.f;
    _activeHeroId = HeroId::END;

    Reset_EditorDesignPositionCache();

    _isOpen = false;
}

HeroId UI_BattleSkillPanel::Resolve_HeroId(PlayerCharacterType playerType) const
{
    switch (playerType)
    {
    case PlayerCharacterType::Gustave:
        return HeroId::Gustave;

    case PlayerCharacterType::Lune:
        return HeroId::Lune;

    case PlayerCharacterType::Maelle:
        return HeroId::Maelle;

    default:
        return HeroId::END;
    }
}

Vec2 UI_BattleSkillPanel::Resolve_HeroTestOffset(HeroId heroId) const
{
    switch (heroId)
    {
    case HeroId::Gustave:
        return _gustaveTestOffset;

    case HeroId::Lune:
        return _luneTestOffset;

    case HeroId::Maelle:
        return _maelleTestOffset;

    case HeroId::END:
    default:
        return Vec2::Zero;
    }
}

void UI_BattleSkillPanel::Apply_CurrentRootEditorOffsets()
{
    if (_root == nullptr || _root->Get_Transform() == nullptr)
        return;

    Apply_EditorDefaultOffsetToRoot();

    Vec2 rootPosition = _root->Get_Transform()->Get_Position();
    rootPosition += Resolve_HeroTestOffset(_activeHeroId);
    _root->Get_Transform()->Set_Position(rootPosition);
}

Unique<UI_BattleSkillPanel> UI_BattleSkillPanel::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleSkillPanel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattleSkillPanel");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_BattleSkillPanel::Free()
{
    Close_Immediately();

    _gustavePanel.reset();
    _lunePanel.reset();
    _maellePanel.reset();
    _gustaveMainPanelUIs.clear();
    _luneMainPanelUIs.clear();
    _maelleMainPanelUIs.clear();
    _tooltipRoots.clear();

    UI_RuntimePanelBase::Free();
}

