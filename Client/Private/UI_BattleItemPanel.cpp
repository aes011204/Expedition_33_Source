#include "UI_BattleItemPanel.h"
#include "Client_Constants.h"
#include "UI_RuntimeLoader.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kBattleItemIntroAnimationName[] = L"BattleSystem_Item_Intro";
    constexpr wchar_t kBattleItemLoopAnimationName[] = L"BattleSystem_Item_Loop";

    float Resolve_BattleItemIntroDurationSec()
    {
        UI_KeyframeAnimationAsset introAnimation{};
        introAnimation.fps = 60;
        introAnimation.startFrame = 0;
        introAnimation.endFrame = 60;
        return Get_KeyframeAnimationDurationSec(introAnimation);
    }
}

HRESULT UI_BattleItemPanel::Initialize(uint32 levelIndex)
{
    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    _mainPanelIntroDuration = Resolve_BattleItemIntroDurationSec();

    Close();

    return S_OK;
}

void UI_BattleItemPanel::Update(float timeDelta)
{
    if (!_isOpen)
        return;

    if (!_isMainPanelOpen || !_isPlayingMainPanelIntro)
        return;

    _mainPanelIntroElapsed += max(0.f, timeDelta);

    if (_mainPanelIntroElapsed < _mainPanelIntroDuration)
        return;

    _isPlayingMainPanelIntro = false;

    if (_root && _root->Is_Open())
        Play_MainPanelAnimation(kBattleItemLoopAnimationName);
}

void UI_BattleItemPanel::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_BattleItemPanel::Close()
{
    _isMainPanelOpen = false;
    _isPlayingMainPanelIntro = false;
    _mainPanelIntroElapsed = 0.f;

    Stop_MainPanelAnimations();
    Close_MainPanelUIs();

    Close_Tooltip();
    UI_RuntimePanelBase::Close();
}

void UI_BattleItemPanel::Open_MainPanel()
{
    Open();
    Open_MainPanelRoot();
}

void UI_BattleItemPanel::Open_Tooltip(BattleItemType itemType)
{
    Open();

    Stop_MainPanelAnimations();
    Close_MainPanelUIs();

    _isMainPanelOpen = false;
    _isPlayingMainPanelIntro = false;
    _mainPanelIntroElapsed = 0.f;

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

void UI_BattleItemPanel::Open_MainPanelRoot()
{
    Close_Tooltip(); // 메인 패널만 키고, 툴팁은 끄기

    if (_root == nullptr)
        return;

    const bool wasMainPanelClosed = !_isMainPanelOpen;
    _isMainPanelOpen = true;

    if (!wasMainPanelClosed)
        return;

    Open_MainPanelUIs(); // ChooseItem에 새로 진입한 순간에만 intro 대상들을 다시 연다.
    _isPlayingMainPanelIntro = true; // ChooseItem에 새로 진입한 프레임에만 intro 1회 재생
    _mainPanelIntroElapsed = 0.f;
    if (!Play_MainPanelAnimation(kBattleItemIntroAnimationName))
    {
        _isPlayingMainPanelIntro = false;
        Play_MainPanelAnimation(kBattleItemLoopAnimationName);
    }
}

void UI_BattleItemPanel::Open_MainPanelUIs()
{
    for (const Shared<UI_Base>& ui : _mainPanelUIs)
    {
        if (ui)
            ui->Open();
    }
}

void UI_BattleItemPanel::Close_MainPanelUIs()
{
    for (const Shared<UI_Base>& ui : _mainPanelUIs)
    {
        if (ui)
            ui->Close();
    }
}

void UI_BattleItemPanel::Stop_MainPanelAnimations()
{
    for (const Shared<UI_Base>& ui : _mainPanelUIs)
    {
        if (ui)
            ui->Stop_UIAnimation();
    }
}

bool UI_BattleItemPanel::Play_MainPanelAnimation(const wchar_t* animationName)
{
    bool played = false;

    for (const Shared<UI_Base>& ui : _mainPanelUIs)
    {
        if (ui == nullptr)
            continue;

        played = ui->Play_UIAnimation(animationName, true) || played;
    }

    return played;
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
    _mainPanelUIs.clear();

    auto Load_AndMerge = [&](const wchar_t* jsonPath, const wchar_t* prototypeTagPrefix) -> HRESULT
    {
        UI_RuntimeLoader::UI_LoadDesc desc{};
        desc.levelIndex = levelIndex;
        desc.jsonPath = jsonPath;
        if (wstring(jsonPath) == Constants::UI::UI_Battle_ItemPanel_Path)
        {
            desc.animationPaths.push_back(Constants::UI_Anim::UI_BattleItemIntroAnim_Path);
            desc.animationPaths.push_back(Constants::UI_Anim::UI_BattleItemLoopAnim_Path);
        }
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

    _mainPanelUIs.clear();
    _mainPanelUIs.reserve(6);

    auto Add_MainPanelAnimTarget = [this](const wchar_t* name, bool required = true) -> HRESULT
        {
            Shared<UI_Base> ui = Find_UI(_uiByName, name);
            if (ui == nullptr)
            {
                if (required)
                    CHECK_NULL(ui, E_FAIL);

                return S_OK;
            }

            _mainPanelUIs.push_back(ui);
            return S_OK;
        };

    CHECK_FAILED(Add_MainPanelAnimTarget(L"Panel_BattleSystem_Item"), E_FAIL);
    CHECK_FAILED(Add_MainPanelAnimTarget(L"Image_BattleSystem_Item_Intro_Deco_1", false), E_FAIL);
    CHECK_FAILED(Add_MainPanelAnimTarget(L"Image_BattleSystem_Item_Intro_Deco_2", false), E_FAIL);
    CHECK_FAILED(Add_MainPanelAnimTarget(L"Panel_BattleSystem_Item_BG", false), E_FAIL);
    CHECK_FAILED(Add_MainPanelAnimTarget(L"Panel_Healing_Tint"), E_FAIL);
    CHECK_FAILED(Add_MainPanelAnimTarget(L"Panel_Energy_Tint"), E_FAIL);
    CHECK_FAILED(Add_MainPanelAnimTarget(L"Panel_Revive_Tint"), E_FAIL);

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
