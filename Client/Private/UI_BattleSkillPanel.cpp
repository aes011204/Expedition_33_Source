#include "UI_BattleSkillPanel.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
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
}
    
HRESULT UI_BattleSkillPanel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();
    return S_OK;
}

void UI_BattleSkillPanel::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!_isOpen)
        return;
}

void UI_BattleSkillPanel::Open_MainPanel(PlayerCharacterType playerType)
{
    Close_MainPanels();
    Close_Tooltips();

    switch (playerType)
    {
    case PlayerCharacterType::Gustave:
        _root = _gustavePanel;
        break;

    case PlayerCharacterType::Lune:
        _root = _lunePanel;
        break;

    case PlayerCharacterType::Maelle:
        _root = _maellePanel;
        break;

    default:
        _root = nullptr;
        break;
    }

    if (_root == nullptr)
    {
        _isOpen = false;
        return;
    }

    UI_RuntimePanelBase::Open();
}

void UI_BattleSkillPanel::Open_Tooltip(const string& skillKey)
{
    Close_MainPanels();
    Close_Tooltips();

    auto iter = _tooltipRoots.find(skillKey);
    if (iter == _tooltipRoots.end() || iter->second == nullptr)
    {
        _root = nullptr;
        _isOpen = false;
        return;
    }

    _root = iter->second;
    UI_RuntimePanelBase::Open();
}

void UI_BattleSkillPanel::Close()
{
    Close_MainPanels();
    Close_Tooltips();

    _root = nullptr;
    _isOpen = false;
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
    CHECK_FAILED(Load_AndMerge(UI_BattleSkillTooltip_FireRise_Path, L"BattleSkillTooltipFireRise"), E_FAIL);

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
    CHECK_FAILED(Bind_Tooltip("FireRise", L"Panel_BattleSystem_SkillTooltip_FireRise"), E_FAIL);

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

        if (tooltipRoot)
            tooltipRoot->Close();
    }
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
    Close();

    _gustavePanel.reset();
    _lunePanel.reset();
    _maellePanel.reset();
    _tooltipRoots.clear();

    UI_RuntimePanelBase::Free();
}

