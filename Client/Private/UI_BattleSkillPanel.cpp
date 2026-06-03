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
}

HRESULT UI_BattleSkillPanel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);
    CHECK_FAILED(Load_TooltipAnimations(), E_FAIL);

    Close_Immediately();

    return S_OK;
}

void UI_BattleSkillPanel::Update(float timeDelta)
{
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
    Apply_CurrentRootEditorOffsets();
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
}

HRESULT UI_BattleSkillPanel::Load_UI(uint32 levelIndex)
{
    _uiByName.clear();

    auto Load_AndMerge = [&](const wchar_t* jsonPath, const wchar_t* prototypeTagPrefix) -> HRESULT
        {
            UI_RuntimeLoader::UI_LoadDesc desc{};
            desc.levelIndex = levelIndex;
            desc.jsonPath = jsonPath;
            desc.prototypeTagPrefix = prototypeTagPrefix;

            umap<wstring, Shared<UI_Base>> loadedUIByName{};
            desc.outUIByName = &loadedUIByName;

            CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

            for (auto& [name, ui] : loadedUIByName)
                _uiByName[name] = ui;

            return S_OK;
        };

    CHECK_FAILED(Load_AndMerge(UI_Battle_GustaveSkillPanel_Path, L"BattleSkillPanelGustave"), E_FAIL);
    CHECK_FAILED(Load_AndMerge(UI_Battle_LuneSkillPanel_Path, L"BattleSkillPanelLune"), E_FAIL);
    CHECK_FAILED(Load_AndMerge(UI_Battle_MaelleSkillPanel_Path, L"BattleSkillPanelMaelle"), E_FAIL);

    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_LumiereAssault_Path, L"BattleSkillTooltipLumiereAssault"), E_FAIL);
    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_UnleashedCharge_Path, L"BattleSkillTooltipUnleashedCharge"), E_FAIL);
    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_MarkingShot_Path, L"BattleSkillTooltipMarkingShot"), E_FAIL);

    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_Immolation_Path, L"BattleSkillTooltipImmolation"), E_FAIL);
    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_IceGust_Path, L"BattleSkillTooltipIceGust"), E_FAIL);
    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_Thunderfall_Path, L"BattleSkillTooltipThunderfall"), E_FAIL);

    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_OffensiveSwitch_Path, L"BattleSkillTooltipOffensiveSwitch"), E_FAIL);
    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_Percee_Path, L"BattleSkillTooltipPercee"), E_FAIL);
    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_FleureFury_Path, L"BattleSkillTooltipFleureFury"), E_FAIL);
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
    if (_gustavePanel) _gustavePanel->Close();
    if (_lunePanel) _lunePanel->Close();
    if (_maellePanel) _maellePanel->Close();
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
    _tooltipRoots.clear();

    UI_RuntimePanelBase::Free();
}

