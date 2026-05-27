#include "UI_BattleItemPanel.h"
#include "Client_Constants.h"
#include "UI_RuntimeLoader.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_BattleItemPanel::Initialize(uint32 levelIndex)
{
    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();

    return S_OK;
}

void UI_BattleItemPanel::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!_isOpen)
        return;
}

void UI_BattleItemPanel::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_BattleItemPanel::Close()
{
    Close_Tooltip();
    UI_RuntimePanelBase::Close();
}

void UI_BattleItemPanel::Open_MainPanel()
{
    Open();
    Close_Tooltip(); // 메인 패널만 키고, 툴팁은 끄기
}

void UI_BattleItemPanel::Open_Tooltip(BattleItemType itemType)
{
    Open();

    if (_root)
        _root->Close();

    Close_Tooltip();

    switch (itemType)
    {
    case BattleItemType::HealingTint:
        if (_healingTooltipRoot) _healingTooltipRoot->Open();
        break;

    case BattleItemType::EnergyTint:
        if (_energyTooltipRoot) _energyTooltipRoot->Open();
        break;

    case BattleItemType::ReviveTint:
        if (_reviveTooltipRoot) _reviveTooltipRoot->Open();
        break;

    default: ;
    }
}

void UI_BattleItemPanel::Close_Tooltip()
{
    if (_healingTooltipRoot) _healingTooltipRoot->Close();
    if (_energyTooltipRoot) _energyTooltipRoot->Close();
    if (_reviveTooltipRoot) _reviveTooltipRoot->Close();
}

void UI_BattleItemPanel::Refresh_ItemTexts(const Item_Data& healing, const Item_Data& energy, const Item_Data& revive)
{
    if (_healingCountText)
        _healingCountText->Set_Text(to_wstring(max(0, healing.count)));

    if (_energyCountText)
        _energyCountText->Set_Text(to_wstring(max(0, energy.count)));

    if (_reviveCountText)
        _reviveCountText->Set_Text(to_wstring(max(0, revive.count)));

    if (_healingDescValueText)
        _healingDescValueText->Set_Text(to_wstring(max(0, healing.healHp)));

    if (_energyDescValueText)
        _energyDescValueText->Set_Text(to_wstring(max(0, energy.restoreAp)));

    if (_reviveDescValueText)
        _reviveDescValueText->Set_Text(to_wstring(max(0, revive.reviveHp)));

    if (_healingDescUnitText)
        _healingDescUnitText->Set_Text(L"%");

    if (_energyDescUnitText)
        _energyDescUnitText->Set_Text(L"AP");

    if (_reviveDescUnitText)
        _reviveDescUnitText->Set_Text(L"%");

    if (_healingTooltipValueText)
        _healingTooltipValueText->Set_Text(to_wstring(max(0, healing.healHp)));

    if (_energyTooltipValueText)
        _energyTooltipValueText->Set_Text(to_wstring(max(0, energy.restoreAp)));

    if (_reviveTooltipValueText)
        _reviveTooltipValueText->Set_Text(to_wstring(max(0, revive.reviveHp)));

    if (_healingTooltipUnitText)
        _healingTooltipUnitText->Set_Text(L"%");

    if (_energyTooltipUnitText)
        _energyTooltipUnitText->Set_Text(L"AP");

    if (_reviveTooltipUnitText)
        _reviveTooltipUnitText->Set_Text(L"%");
}

HRESULT UI_BattleItemPanel::Load_UI(uint32 levelIndex)
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
        {
            _uiByName[name] = ui;
        }

        return S_OK;
    };

    CHECK_FAILED(Load_AndMerge(Constants::UI::UI_Battle_ItemPanel_Path, L"BattleItemPanel"), E_FAIL);
    CHECK_FAILED(Load_AndMerge(Constants::UI::UI_Battle_HealingTooltip_Path, L"BattleItemHealingTooltip"), E_FAIL);
    CHECK_FAILED(Load_AndMerge(Constants::UI::UI_Battle_EnergyTooltip_Path, L"BattleItemEnergyTooltip"), E_FAIL);
    CHECK_FAILED(Load_AndMerge(Constants::UI::UI_Battle_ReviveTooltip_Path, L"BattleItemReviveTooltip"), E_FAIL);

    return S_OK;
}

HRESULT UI_BattleItemPanel::Bind_UI()
{
    _root = Find_UI(_uiByName, L"Panel_BattleSystem_Item");
    _healingTooltipRoot = Find_UI(_uiByName, L"Panel_BattleSystem_ItemTooltip_Healing_Tint");
    _energyTooltipRoot = Find_UI(_uiByName, L"Panel_BattleSystem_ItemTooltip_Energy_Tint");
    _reviveTooltipRoot = Find_UI(_uiByName, L"Panel_BattleSystem_ItemTooltip_Revive_Tint");
    _healingCountText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Healing_Tint_Count");
    _energyCountText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Energy_Tint_Count");
    _reviveCountText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Revive_Tint_Count");
    _healingDescValueText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Healing_Tint_Description_2");
    _energyDescValueText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Energy_Tint_Description_2");
    _reviveDescValueText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Revive_Tint_Description_2");
    _healingDescUnitText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Healing_Tint_Description_3");
    _energyDescUnitText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Energy_Tint_Description_3");
    _reviveDescUnitText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Revive_Tint_Description_3");
    _healingTooltipValueText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleSystem_ItemTooltip_Healing_Tint_Number_1");
    _energyTooltipValueText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleSystem_ItemTooltip_Energy_Tint_Number_1");
    _reviveTooltipValueText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleSystem_ItemTooltip_Revive_Tint_Number_1");
    _healingTooltipUnitText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleSystem_ItemTooltip_Healing_Tint_Percent");
    _energyTooltipUnitText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleSystem_ItemTooltip_Energy_Tint_AP");
    _reviveTooltipUnitText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleSystem_ItemTooltip_Revive_Tint_Percent");

    CHECK_NULL(_root, E_FAIL);
    CHECK_NULL(_healingTooltipRoot, E_FAIL);
    CHECK_NULL(_energyTooltipRoot, E_FAIL);
    CHECK_NULL(_reviveTooltipRoot, E_FAIL);
    CHECK_NULL(_healingCountText, E_FAIL);
    CHECK_NULL(_energyCountText, E_FAIL);
    CHECK_NULL(_reviveCountText, E_FAIL);
    CHECK_NULL(_healingDescValueText, E_FAIL);
    CHECK_NULL(_energyDescValueText, E_FAIL);
    CHECK_NULL(_reviveDescValueText, E_FAIL);
    CHECK_NULL(_healingDescUnitText, E_FAIL);
    CHECK_NULL(_energyDescUnitText, E_FAIL);
    CHECK_NULL(_reviveDescUnitText, E_FAIL);
    CHECK_NULL(_healingTooltipValueText, E_FAIL);
    CHECK_NULL(_energyTooltipValueText, E_FAIL);
    CHECK_NULL(_reviveTooltipValueText, E_FAIL);
    CHECK_NULL(_healingTooltipUnitText, E_FAIL);
    CHECK_NULL(_energyTooltipUnitText, E_FAIL);
    CHECK_NULL(_reviveTooltipUnitText, E_FAIL);

    return S_OK;
}

Unique<UI_BattleItemPanel> UI_BattleItemPanel::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleItemPanel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattleItemPanel");
        return nullptr;
    }

    return instance;
}

void UI_BattleItemPanel::Free()
{
    UI_RuntimePanelBase::Free();
}
