#include "SkillTree_Panel.h"
#include <filesystem>
#include "Client_Constants.h"
#include "ClientInstance.h"
#include "EffectCom.h"
#include "Player_WorldBase.h"
#include "PlayerStatCom.h"
#include "SkillTreeCom.h"
#include "Spawn_Prefab.h"
#include "GameInstance.h"
#include "LightingPipeline.h"
#include "PostProcessPipeline.h"
#include "UI_BUtton.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h" 
#include "UI_Text.h"
#include "PreviewCamera.h"
#include "UI_Base.h"
#include "UI_Utils.h"
#include "PreviewRimFeature.h"

#include <algorithm>

#include "PreviewPlayer.h"

using namespace UI_Utils;

namespace
{
    constexpr HeroId characterStatusOrder[] =
    {
        HeroId::Lune,
        HeroId::Gustave,
        HeroId::Maelle,
    };

    uint32 Get_CharacterStatusIndex(HeroId heroId)
    {
        for(uint32 i = 0; i < _countof(characterStatusOrder); ++i)
        {
            if(characterStatusOrder[i] == heroId)
                return i;
        }
        return 0;
    }

    HeroId Get_SelectedCharacterStatusHeroId(uint32 selectedHeroIndex)
    {
        if(selectedHeroIndex >= _countof(characterStatusOrder))
            return HeroId::Gustave;
        return  characterStatusOrder[selectedHeroIndex];
    }
}

SkillTree_Panel::SkillTree_Panel()
{
}

SkillTree_Panel::~SkillTree_Panel()
{
}

HRESULT SkillTree_Panel::Initialize(uint32 levelIndex)
{
    SkillTreeViewState luneView, gustaveView, maelleView {};

    _levelIndex = levelIndex;

    CHECK_FAILED(Load_Background_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Load_UI(HeroId::Lune, levelIndex, L"Lune", Constants::UI::UI_Lune_SkillTree_Path, Constants::UI::UI_Lune_SkillTooltips_Dir, L"Panel_Lune_SkillTree", L"Lune", L"Panel_CharacterSelect_01",Constants::UI::UI_Lune_CharacterSheet_Dir, luneView), E_FAIL);;
    CHECK_FAILED(Load_UI(HeroId::Gustave, levelIndex, L"Gustave", Constants::UI::UI_Gustave_SkillTree_Path, Constants::UI::UI_Gustave_SkillTooltips_Dir, L"Panel_Gustave_SkillTree", L"Gustave", L"Panel_CharacterSelect_02",Constants::UI::UI_Gustave_CharacterSheet_Dir, gustaveView), E_FAIL);
    CHECK_FAILED(Load_UI(HeroId::Maelle, levelIndex, L"Maelle", Constants::UI::UI_Maelle_SkillTree_Path, Constants::UI::UI_Maelle_SkillTooltips_Dir, L"Panel_Maelle_SkillTree", L"Maelle", L"Panel_CharacterSelect_03", Constants::UI::UI_Maelle_CharacterSheet_Dir,maelleView), E_FAIL);

    _viewByPlayerId.emplace(HeroId::Lune, luneView);
    _viewByPlayerId.emplace(HeroId::Gustave, gustaveView);
    _viewByPlayerId.emplace(HeroId::Maelle, maelleView);

    _currentView = &_viewByPlayerId[HeroId::Lune];

    Bind_Status_UI();
    Set_SkillPoint();

    _baseSize = _currentView->skillNodes.front().image->Get_Transform()->Get_Scale();
    _hoveredSize = _baseSize * 1.6f;
    Close();

    return S_OK;
}

void SkillTree_Panel::Update(float timeDelta)
{
    if(!_isOpen)
        return;

   Handle_SelectionInput();
   Update_UI();
}

void SkillTree_Panel::Open()
{
    const bool wasOpen = _isOpen;

    _isOpen = true;

    if (!wasOpen)
        GAME->Play_SoundCue(Constants::SoundCue::UI_Global_GenericEnter);

    for(auto& [heroId, view] : _viewByPlayerId)
    {
        UNREFERENCED_PARAMETER(heroId);

        for(auto& node : view.skillNodes)
            node.wasHovered = false;

        for(auto& lockNode : view.lockedNodes)
            lockNode.wasHovered = false;
    }

    //바뀌지 않는 배경 UI
    if(_backgroundUI)
        _backgroundUI->Open();

    //선택된 캐릭터 ui 확대
    _currentView->characterSelectUI->Get_Transform()->Set_Scale(_hoveredSize);

    CLIENT->Begin_Preview(PreviewShotPanel::SkillTree);

    auto previewLayout  = Build_PreviewLayout();
    CLIENT->Apply_PreviewLayout(previewLayout);
    CLIENT->Show_Preview(Get_SelectedCharacterStatusHeroId(_selectedHeroIndex),PreviewShotPanel::SkillTree);
    Update_PreviewHeroVisibility();
    Apply_PreviewOverrides();

    // 마우스 잠금
    GAME->Unlock_Mouse();
    GAME->Set_GameplayPaused(true);
}

void SkillTree_Panel::Close()
{
    _isOpen = false;
    Restore_PreviewOverrides();

    if(_backgroundUI)
        _backgroundUI->Close();

    _currentView->characterSelectUI->Get_Transform()->Set_Scale(_baseSize);

    for(auto i = 0 ; i < static_cast<int>(HeroId::END) ; i++)
    {
        _viewByPlayerId[static_cast<HeroId>(i)].root->Close();

       for(auto tooltip : _viewByPlayerId[static_cast<HeroId>(i)].tooltips)
       {
           tooltip.second->Close();
       }

        for(auto node :_viewByPlayerId[static_cast<HeroId>(i)].skillNodes)
        {
            if(node.unlockedPopup)
                node.unlockedPopup->Close();

            if(node.canUnlockPopup)
                node.canUnlockPopup->Close();

            if(node.lockedPopup)
                node.lockedPopup->Close();
        }

        for(auto node :_viewByPlayerId[static_cast<HeroId>(i)].lockedNodes)
        {
            if(node.lockedPopup)
                node.lockedPopup->Close();
        }

        
        _viewByPlayerId[static_cast<HeroId>(i)].characterSheet->Close();

    }
    CLIENT->Hide_Preview();

    GAME->Set_GameplayPaused(false);
    GAME->Lock_Mouse();
}

void SkillTree_Panel::Toggle()
{
    if(_isOpen)
        Close();
    else
        Open();
}

void SkillTree_Panel::Handle_SelectionInput()
{
    const vector<HeroPartyEntry>& partyEntries = CLIENT->Get_HeroEntries();

    if(partyEntries.empty())
        return;

    const uint32 heroCount = static_cast<uint32>(partyEntries.size());
    uint32 previousIndex = _selectedHeroIndex;

    if(GAME->KeyDown(KEY_TYPE::Q))
    {
        _selectedHeroIndex = (_selectedHeroIndex + heroCount - 1) % heroCount;
    }

    if(GAME->KeyDown(KEY_TYPE::E))
    {
        _selectedHeroIndex = (_selectedHeroIndex + 1) % heroCount;
    }

    if(previousIndex != _selectedHeroIndex)
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_SelectAlly);

        Refresh_CharacterSelectUI();
        Change_ToSelectedViewState();
        Set_SkillPoint();
        Refresh_CombatStats();

  /*      auto previewLayout = Build_PreviewLayout();
        CLIENT->Apply_PreviewLayout(previewLayout);*/
        CLIENT->Show_Preview(Get_SelectedCharacterStatusHeroId(_selectedHeroIndex), PreviewShotPanel::SkillTree);
        Update_PreviewHeroVisibility();
        Apply_PreviewOverrides();
    }
}

HRESULT SkillTree_Panel::Load_Background_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc {};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_SkillTree_BG_Path;
    desc.prototypeTagPrefix = L"SkillTree_BG";
    desc.outUIByName = &_backgroundUIs;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _backgroundUI = Find_UI(_backgroundUIs,L"Panel_SkillTree");
    _skillpointUI = Find_UI(_backgroundUIs,L"Text_CurrentSkillPoint");

    CHECK_NULL(_backgroundUI, E_FAIL);

    return S_OK;
}

HRESULT SkillTree_Panel::Load_UI(HeroId heroId, uint32 levelIndex, const wstring& playerName,const wstring& skillTreeJsonPath, const wstring& toolTipJsonPath, const wstring& rootName, const wstring& prototypeTagPrefix,const wstring& characterSelectUI, const wstring& characterJsonPath, SkillTreeViewState& outView)
{
    SkillTreeViewState viewState{};

    UI_RuntimeLoader::UI_LoadDesc desc {};
    desc.levelIndex = levelIndex;
    desc.jsonPath = skillTreeJsonPath;
    desc.prototypeTagPrefix = prototypeTagPrefix + L"SkillTree";
    desc.outUIByName = &outView.uiByName;

    outView.heroId = heroId;
    outView.characterSelectUI = Find_UI(_backgroundUIs, characterSelectUI);

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    outView.root = Find_UI(outView.uiByName, rootName);

    Load_UI_ToolTips(prototypeTagPrefix,toolTipJsonPath, levelIndex, playerName, outView);
    Load_CharacterSheet(levelIndex,characterJsonPath, playerName, outView);
    Bind_SkillNode_UI(playerName, outView);

    if(heroId != HeroId::Lune)
    Bind_LockNode_UI(playerName, outView);

    auto hero = CLIENT->Find_Hero(heroId);

    CHECK_NULL(hero, E_FAIL);

    outView.skillTreeCom = hero->Get_Component<SkillTreeCom>();

    CHECK_NULL(outView.skillTreeCom, E_FAIL);

    return S_OK;
}

HRESULT SkillTree_Panel::Load_CharacterSheet(uint32 levelIndex,const wstring& jsonPath, const wstring& playerName, SkillTreeViewState& outView)
{
    UI_RuntimeLoader::UI_LoadDesc desc {};
    desc.levelIndex = levelIndex;
    desc.jsonPath = jsonPath;
    desc.prototypeTagPrefix = L"CharacterSheet_" + playerName;
    desc.outUIByName = &outView.characterSheetUIs;
    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);
    outView.characterSheet = Find_UI(outView.characterSheetUIs,L"Panel_CharacterSheet");

    return S_OK;
}

HRESULT SkillTree_Panel::Load_UI_ToolTips(const wstring& prototypeTagPrefix, const wstring& toolTipJsonPath, uint32 levelIndex, const wstring& playerName, SkillTreeViewState& outView)
{
    UI_RuntimeLoader::UI_LoadDesc desc {};
    desc.levelIndex = levelIndex;
    desc.jsonPath = toolTipJsonPath;
    desc.prototypeTagPrefix = prototypeTagPrefix + L"Tooltip";
    desc.outUINamePrefixFilter = L"Panel_SkillTooltip_" + playerName + L"_";
    desc.outUIByName = &outView.tooltipsByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    for(const auto& [name, ui] : outView.tooltipsByName)
    {
        if(name.rfind(L"Panel_SkillTooltip_" + playerName, 0) != 0)
            continue;

        const wstring  prefix =  L"Panel_SkillTooltip_"+ playerName + L"_";
        const wstring skillId = name.substr(wcslen(prefix.c_str()));
        outView.tooltips.emplace(skillId, ui);
    }
    return S_OK;
}

HRESULT SkillTree_Panel::Bind_SkillNode_UI(const wstring& playerName, SkillTreeViewState& outView)
{
    const wstring buttonPrefix = L"Button_Skill_" + playerName + L"_";
    const wstring imagePrefix = L"Image_Skill_" + playerName +  L"_";
    const wstring textPrefix = L"Text_Skill_" + playerName + L"_";

    for(const auto& [name, ui] : outView.uiByName)
    {
        Shared<UI_Button> button = dynamic_pointer_cast<UI_Button>(ui);

        if(button == nullptr)
            continue;

        if(name.rfind(buttonPrefix, 0) != 0)
            continue;

        const wstring skillIdW = name.substr(buttonPrefix.size());
        const string  skillId = String::ToString(skillIdW);
        const wstring imageId = imagePrefix + skillIdW;
        const wstring textId = textPrefix + skillIdW + L"_Cost";
        const wstring lockedPopupId = L"Panel_Skill_" + playerName + L"_" + skillIdW + L"_Lock";
        const wstring unlockedPopupId =  L"Panel_Skill_" + playerName + L"_" + skillIdW + L"_On"; 
        const wstring canUnlockPopupId =  L"Panel_Skill_" + playerName + L"_" + skillIdW + L"_CanUnlock";

        Shared<UI_Image> icon = dynamic_pointer_cast<UI_Image>(Find_UI(outView.uiByName, imageId));
        Shared<UI_Text>  costText = dynamic_pointer_cast<UI_Text>(Find_UI(outView.uiByName, textId));
        Shared<UI_Base>  lockedPopup = dynamic_pointer_cast<UI_Base>(Find_UI(outView.uiByName, lockedPopupId));
        Shared<UI_Base>  unlockedPopup = dynamic_pointer_cast<UI_Base>(Find_UI(outView.uiByName, unlockedPopupId));
        Shared<UI_Base>  canUnlockPopup = dynamic_pointer_cast<UI_Base>(Find_UI(outView.uiByName, canUnlockPopupId));

        SkillNodeUI node{};
        node.skillId = skillId;
        node.button = button;
        node.image = icon;
        node.lockedPopup = lockedPopup;
        node.unlockedPopup = unlockedPopup;
        node.canUnlockPopup = canUnlockPopup;
        node.costText = costText;

        outView.skillNodes.push_back(node);
    }
    return S_OK;
}

HRESULT SkillTree_Panel::Bind_LockNode_UI(const wstring& playerName,SkillTreeViewState& outView)
{
    const wstring buttonPrefix = L"Button_" + playerName + L"_Locked_";
    
    for(const auto& [name, ui] : outView.uiByName)
    {
        if(!name.starts_with(buttonPrefix))
            continue;

        Shared<UI_Button> button = dynamic_pointer_cast<UI_Button>(ui);
        if(button == nullptr)
            continue;

        const wstring nodeNameW = name.substr(buttonPrefix.size());
        const wstring panelId = L"Panel_" + playerName + L"_Locked_" + nodeNameW;
        const wstring imageId = L"Image_" + playerName + L"_Locked_" + nodeNameW + L"_Mask";
        const string  nodeName = String::ToString(nodeNameW);

        Shared<UI_Image>  image = dynamic_pointer_cast<UI_Image>(Find_UI(outView.uiByName, imageId));
        Shared<UI_Base>   lockedPopup = Find_UI(outView.uiByName, panelId);

        LockNodeUI node {};
        node.button = button;
        node.image = image;
        node.lockedPopup = lockedPopup;
        node.name = nodeName;

        outView.lockedNodes.push_back(node);
    }
    return S_OK;
}

HRESULT SkillTree_Panel::Bind_Status_UI()
{
    //한 번만 처음 세팅하는거니까 플레이어 3명 한번에 해야함
    for(auto i = 0 ; i < static_cast<int>(HeroId::END) ; i++)
    {
        _viewByPlayerId[static_cast<HeroId>(i)].levelText = Find_TypedUI<UI_Text>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_CurrentLevel");
        _viewByPlayerId[static_cast<HeroId>(i)].vitalityText = Find_TypedUI<UI_Text>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_CurrentVitality");
        _viewByPlayerId[static_cast<HeroId>(i)].mightText= Find_TypedUI<UI_Text>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_CurrentMight");
        _viewByPlayerId[static_cast<HeroId>(i)].agilityText = Find_TypedUI<UI_Text>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_CurrentAgility");
        _viewByPlayerId[static_cast<HeroId>(i)].defenseStatText = Find_TypedUI<UI_Text>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_CurrentDefense");
        _viewByPlayerId[static_cast<HeroId>(i)].luckText = Find_TypedUI<UI_Text>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_CurrentLuck");

        _viewByPlayerId[static_cast<HeroId>(i)].curExpText= Find_TypedUI<UI_Text>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_Character_Current_EXP");
        _viewByPlayerId[static_cast<HeroId>(i)].maxExpText = Find_TypedUI<UI_Text>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_Character_Max_EXP");
        _viewByPlayerId[static_cast<HeroId>(i)].expGaugeImage = Find_TypedUI<UI_Image>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_Character_EXP_Gauge");
        _viewByPlayerId[static_cast<HeroId>(i)].curHpText = Find_TypedUI<UI_Text>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_Character_Current_HP");
        _viewByPlayerId[static_cast<HeroId>(i)].maxHpText = Find_TypedUI<UI_Text>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_Character_Max_HP");
        _viewByPlayerId[static_cast<HeroId>(i)].hpGaugeImage = Find_TypedUI<UI_Image>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_Character_HP_Gauge");

        _viewByPlayerId[static_cast<HeroId>(i)].hpText = Find_TypedUI<UI_Text>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_CurrentStatHP");
        _viewByPlayerId[static_cast<HeroId>(i)].attackText = Find_TypedUI<UI_Text>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_CurrentStatAttack");
        _viewByPlayerId[static_cast<HeroId>(i)].speedText = Find_TypedUI<UI_Text>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_CurrentStatSpeed");
        _viewByPlayerId[static_cast<HeroId>(i)].defenseValueText = Find_TypedUI<UI_Text>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_CurrentStatDefense");
        _viewByPlayerId[static_cast<HeroId>(i)].criticalRateText = Find_TypedUI<UI_Text>(_viewByPlayerId[static_cast<HeroId>(i)].characterSheetUIs, L"Text_CurrentStatCriticalPercent");

        if (_viewByPlayerId[static_cast<HeroId>(i)].expGaugeImage)
            _viewByPlayerId[static_cast<HeroId>(i)].expGaugeImage->Set_UseFillRatio(true); // SkillTree character sheet도 UV crop 방식으로 경험치 바를 채운다.

        if (_viewByPlayerId[static_cast<HeroId>(i)].hpGaugeImage)
            _viewByPlayerId[static_cast<HeroId>(i)].hpGaugeImage->Set_UseFillRatio(true);  // HP 바도 같은 방식으로 채운다.
    }

    return S_OK;
}

void SkillTree_Panel::Set_SkillPoint()
{
    for (auto& node : _currentView->skillNodes)
    {
        const auto desc = CLIENT->Get_SkillTreeDesc(node.skillId);

        if(!desc)
           continue;

        if(node.costText)
        node.costText->Set_Text(to_wstring(desc->unlockCost));
    }

    auto hero = CLIENT->Find_Hero(_currentView->heroId);

    CHECK_NULL(hero);

    auto playerStatCom = hero->Get_Component<PlayerStatCom>();

    CHECK_NULL(playerStatCom);

    dynamic_pointer_cast<UI_Text>(_skillpointUI)->Set_Text(to_wstring(playerStatCom->Get_PlayerRuntimeStat().CurSkillPoint));
}


void SkillTree_Panel::Update_UI()
{
     Vec4 redFont = Vec4(186.f / 255.f, 40.f / 255.f, 40.f / 255.f, 1.f);
    Vec4 grayFont = Vec4(170.f / 255.f, 170.f / 255.f, 170.f / 255.f, 1.f);
    Vec4 yellowFont = Vec4(235.f / 255.f, 207.f / 255.f, 103.f / 255.f, 1.f);

    if(!_isOpen)
        return;

    if(_currentView->characterSheet && !_currentView->characterSheet->Is_Open())
        _currentView->characterSheet->Open();

    for(auto i = 0 ; i < static_cast<int>(HeroId::END) ; i++)
    {
        _viewByPlayerId[static_cast<HeroId>(i)].root->Close();

        for(auto tooltip : _viewByPlayerId[static_cast<HeroId>(i)].tooltips)
        {
            tooltip.second->Close();
        }

        for(auto node :_viewByPlayerId[static_cast<HeroId>(i)].skillNodes)
        {
            if(node.unlockedPopup)
                node.unlockedPopup->Close();

            if(node.canUnlockPopup)
                node.canUnlockPopup->Close();

            if(node.lockedPopup)
                node.lockedPopup->Close();
        }
    }

    if(_currentView->root && !_currentView->root->Is_Open())
        _currentView->root->Open();

    for( auto node :_currentView->lockedNodes)
    {
        if(node.lockedPopup)
            node.lockedPopup->Close();    

        if(node.image)
            node.image->Get_Transform()->Set_Scale(_baseSize);
    }

    for(auto& node : _currentView->skillNodes)
    {
        if(node.image)
           node.image->Set_GrayScaleEnabled(false);

        if(node.costText)
        {
           node.costText->Close();
           node.costText->Set_Color(grayFont);
        }

        if(node.image)
        node.image->Get_Transform()->Set_Scale(_baseSize);

        if(node.canUnlockPopup)
        node.canUnlockPopup ->Close();

        if(node.lockedPopup)
        node.lockedPopup->Close();

        if(node.unlockedPopup)
        node.unlockedPopup->Close();

        const bool isUnlocked = _currentView->skillTreeCom->IsUnlocked(node.skillId);


        const bool canUnlock = !isUnlocked && _currentView->skillTreeCom->CanUnlock(node.skillId);

        if(node.button->Is_Pressed())
        {
            // 클릭했을 때 모든 버튼 오버레이 처리

            if(node.image)
                node.image->Set_OverlayEnabled(true);
        }
        else
        {
            if(node.image)
                node.image->Set_OverlayEnabled(false);
        }

        //해금이 되지 않았을 떄
        if(!isUnlocked)
        {
           // //이미지는 회색으로

            if(node.skillId=="Mezzoforte")
                int a = 10;

            if(node.image)
            {
                node.image->Set_GrayScaleEnabled(true);
                node.image->Set_GrayAmount(1);
            }

            if(!_currentView->skillTreeCom->HasRequiredSkill(node.skillId))
            {
                //잠겨있는 상태 & 선행스킬 없을 때
                //TODO 회색으로 바꾸기
                if(node.costText)
                {
                    node.costText->Open();
                    node.costText->Set_Color(grayFont);  
                }
            }
            else if(_currentView->skillTreeCom->HasRequiredSkill(node.skillId) && !_currentView->skillTreeCom->HasEnoughSkillPoint(node.skillId) )
            {
                //잠겨있는 상태인데 선행스킬은 있는데 포인트가 부족할 때
               //TODO 빨간색으로 바꾸기
                if(node.costText)
                {
                    node.costText->Open();
                    node.costText->Set_Color(redFont);
                }
            }
            else if(canUnlock)
            {
                //잠겨 있는 상태인데 해금 가능한 상태일 때(선행스킬 + 포인트)
                if(node.costText)
                {
                    node.costText->Open();
                    node.costText->Set_Color(yellowFont);
                }
            }
        }

        auto ui = _currentView->tooltips.find(String::ToWString(node.skillId));

        //hover일때
        //TODO 이건 anchor 생기면 다시 보정
        //공통적으로 약간 커짐 && 툴팁 띄우기
        const bool isHovered = node.button->Is_Hovered();

        if(!node.wasHovered && isHovered)
            GAME->Play_SoundCue(Constants::SoundCue::UI_SkillTree_Navigate);

        node.wasHovered = isHovered;

        if(isHovered)
        {
            if(node.image)
            node.image->Get_Transform()->Set_Scale(_hoveredSize);
 
            if(ui != _currentView->tooltips.end() && ui->second)
            ui->second->Open();

            if(isUnlocked)
            {
                if(node.unlockedPopup)
                node.unlockedPopup->Open();
            }
            else
            {
                if(node.costText)
                node.costText->Open();

                if(canUnlock)
                {
                    if(node.canUnlockPopup)
                        node.canUnlockPopup->Open();

                    if(GAME->KeyDown(KEY_TYPE::F))
                    {
                        const bool wasUnlocked = _currentView->skillTreeCom->IsUnlocked(node.skillId);
                        _currentView->skillTreeCom->TryUnlock(node.skillId);

                        if(!wasUnlocked && _currentView->skillTreeCom->IsUnlocked(node.skillId))
                            GAME->Play_SoundCue(Constants::SoundCue::UI_SkillTree_Learn);

                        Set_SkillPoint();
                    }
                }
                else
                {
                    if(node.lockedPopup)
                        node.lockedPopup->Open();

                    if(GAME->KeyDown(KEY_TYPE::F))
                        GAME->Play_SoundCue(Constants::SoundCue::UI_SkillTree_Blocked);
                }
            }
        }
    }

    for(auto& lockNode : _currentView->lockedNodes)
    {
        if(lockNode.button->Is_Pressed())
        {
            if(lockNode.image)
                lockNode.image->Set_OverlayEnabled(true);
        }
        else
        {
            if(lockNode.image)
              lockNode.image->Set_OverlayEnabled(false);
        }

        const bool isHovered = lockNode.button->Is_Hovered();

        if(!lockNode.wasHovered && isHovered)
            GAME->Play_SoundCue(Constants::SoundCue::UI_SkillTree_Navigate);

        lockNode.wasHovered = isHovered;

        if(isHovered)
        {
            if(lockNode.image)
                lockNode.image->Get_Transform()->Set_Scale(_hoveredSize);
            
             if(lockNode.lockedPopup)
                 lockNode.lockedPopup->Open();
        }
    }
}

void SkillTree_Panel::Refresh_CharacterSelectUI()
{
    static const Vec2 selectedScale = Vec2(1.4f, 1.4f);
    static const Vec2 normalScale = Vec2(1.f, 1.f);

    for(auto i = 0 ; i < static_cast<int>(HeroId::END) ; i++)
    {
        HeroId heroId = characterStatusOrder[i];
        const auto& selectedUI = _viewByPlayerId[heroId].characterSelectUI;
        const bool isSelected = (i == _selectedHeroIndex);
        const Vec2 targetScale = isSelected ? selectedScale : normalScale;

        if(selectedUI->Get_Transform() != nullptr)
        {
            selectedUI->Get_Transform()->Set_Scale(targetScale);
        }
    }
}

void SkillTree_Panel::Refresh_CombatStats()
{
    Shared<Player_WorldBase> player = CLIENT->Get_Hero(static_cast<HeroPartySlot>(_selectedHeroIndex));
    CHECK_NULL(player);

    Shared<PlayerStatCom> playerStat = player->Get_Component<PlayerStatCom>();
    CHECK_NULL(playerStat);

    const PlayerStatCom:: PRuntimeCombatStat& statInfo = playerStat->Get_PlayerRuntimeStat();
    const uint32 maxExp = statInfo.CurMaxExp > 0 ? statInfo.CurMaxExp : max(1u, statInfo.nextExp);
    const float expRatio = std::clamp(
        maxExp > 0 ? static_cast<float>(statInfo.CurExp) / static_cast<float>(maxExp) : 0.f,
        0.f,
        1.f);
    const float hpRatio = std::clamp(
        statInfo.CurMaxHp > 0 ? static_cast<float>(statInfo.CurHp) / static_cast<float>(statInfo.CurMaxHp) : 0.f,
        0.f,
        1.f);

    Set_UIntText(_currentView->levelText, statInfo.CurLevel);

    Set_IntText(_currentView->vitalityText, statInfo.CurVitality);
    Set_IntText(_currentView->mightText, statInfo.CurMight);
    Set_IntText(_currentView->agilityText, statInfo.CurAgility);
    Set_IntText(_currentView->defenseStatText, statInfo.CurDefense);
    Set_IntText(_currentView->luckText, statInfo.CurLuck);

    Set_IntText(_currentView->curExpText, statInfo.CurExp);
    Set_UIntText(_currentView->maxExpText, maxExp);
    Set_IntText(_currentView->curHpText, static_cast<int32>(statInfo.CurHp));
    Set_IntText(_currentView->maxHpText, static_cast<int32>(statInfo.CurMaxHp));

    if (_currentView->expGaugeImage)
        _currentView->expGaugeImage->Set_FillRatio(expRatio); // 현재 경험치 / 다음 레벨 필요 경험치 비율

    if (_currentView->hpGaugeImage)
        _currentView->hpGaugeImage->Set_FillRatio(hpRatio);   // 현재 HP / 최대 HP 비율

    Set_HpText(_currentView->hpText, statInfo.CurHp);
    Set_IntText(_currentView->attackText, static_cast<int32>(statInfo.CurPhysicalAttack));
    Set_IntText(_currentView->speedText, static_cast<int32>(statInfo.CurSpeed));
    Set_IntText(_currentView->defenseValueText, static_cast<int32>(statInfo.CurPhysicalDefense));
    Set_PercentText(_currentView->criticalRateText, statInfo.CurCriticalRate);
}

void SkillTree_Panel::Set_IntText(const Shared<UI_Text>& text, int32 value)
{
    if(text)
        text->Set_Text(to_wstring(value));
}

void SkillTree_Panel::Set_UIntText(const Shared<UI_Text>& text, uint32 value)
{
    if(text)
        text->Set_Text(to_wstring(value));
}

void SkillTree_Panel::Set_PercentText(const Shared<UI_Text>& text, float value)
{
    if(text)
    {
        const int32 criticalRate = static_cast<int32>(value);
        text->Set_Text(to_wstring(criticalRate));
    }
}

void SkillTree_Panel::Set_HpText(const Shared<UI_Text>& text, float hp)
{
    if(text)
    {
        const int32 hpValue = static_cast<int32>(hp);
        text->Set_Text(to_wstring(hpValue));
    }
}

void SkillTree_Panel::Change_ToSelectedViewState()
{
    if(_currentView->root)
        _currentView->root->Close();

    if(_currentView->characterSheet)
        _currentView->characterSheet->Close();

    _currentView = &_viewByPlayerId[Get_SelectedCharacterStatusHeroId(_selectedHeroIndex)];
}

vector<UIPreviewManager::PreviewSlotDesc> SkillTree_Panel::Build_PreviewLayout()
{
    static const Vec3 positions[] =
    {
        //Vec3(1.1f,  0.f, -0.1f),
        //Vec3(0.0f,  0.f, 0.f),
        //Vec3(-1.1f, 0.f, -0.1f),
        Vec3(0.8f,  0.f,  0.0f),
        Vec3(0.0f,  0.f,  0.2f),
        Vec3(-1.1f, 0.f, -0.3f),
    };

    static const Vec3 rotations[] =
    {
        Vec3(0.f, 0.f, -0.1f),
        Vec3(0.f, 0.f, 0.f),
        Vec3(0.f, 0.f, -0.1f),
    };

    vector<UIPreviewManager::PreviewSlotDesc> layout;
    layout.reserve(_countof(characterStatusOrder));

    for (uint32 i = 0; i < _countof(characterStatusOrder); ++i)
    {
        layout.push_back({ characterStatusOrder[i], positions[i], rotations[i] });
    }

    return layout;
}

void SkillTree_Panel::Apply_PreviewOverrides()
{
    // 조명 등 변경
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

        // 기존 상태 저장 한 번만.
    if (_previewOverrideState.isCaptured == false)
    {
        // SkillTree 닫을 때 월드 조명 값이 남지 않도록 원본은 한 번만 잡아 둔다.
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

    // 빛 세기 및 방향 변경
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

    // RimRay 효과 적용
    Apply_PreviewRimState();
}

// 스킬트리 닫을 때 preview 때문에 바꿔놨던 값을 원래 월드 상태로 되돌리는 복원 함수
void SkillTree_Panel::Restore_PreviewOverrides()
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

// 선택된 캐릭터에 Rim 효과 적용, 나머지는 해제
void SkillTree_Panel::Apply_PreviewRimState()
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
        // 화면에 남겨 둔 preview hero는 전부 같은 rim 톤을 쓴다.
        if (previewHero.player->Is_Visible())
            effectCom->Set_PreviewRim(
                kPreviewRimColor,
                kPreviewRimIntensity,
                kPreviewRimPower,
                kPreviewHairRimIntensity);
        else // 나머지는 해제
            effectCom->Clear_PreviewRim();
    }
}

//  preview hero들에 걸린 rim 상태를 전부 해제
void SkillTree_Panel::Clear_PreviewRimState()
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

// preview에 떠 있는 hero들 중에서 선택된 캐릭터만 보이게 하고 나머지는 숨기는 함수
void SkillTree_Panel::Update_PreviewHeroVisibility()
{
    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    if (previewManager == nullptr)
        return;

    const HeroId selectedHeroId = Get_SelectedCharacterStatusHeroId(_selectedHeroIndex);

    for (const auto& previewHero : previewManager->Get_PreviewHeroes())
    {
        if (previewHero.player == nullptr)
            continue;

        previewHero.player->Set_Visible(previewHero.heroId == selectedHeroId);
    }
}


Unique<SkillTree_Panel> SkillTree_Panel::Create(uint32 levelIndex)
{
    auto instance = make_unique<SkillTree_Panel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : SkillTree");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void SkillTree_Panel::Free()
{
    Restore_PreviewOverrides();
    __super::Free();
}
