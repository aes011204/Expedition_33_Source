#include "SkillTree_Panel.h"
#include <filesystem>

#include "Client_Constants.h"
#include "ClientInstance.h"
#include "Player.h"
#include "SkillTreeCom.h"
#include "UI_BUtton.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h" 
#include "UI_Text.h"

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
    CHECK_FAILED(Load_UI(HeroId::Lune, levelIndex, L"Lune", Constants::UI::UI_Lune_SkillTree_Path, Constants::UI::UI_Lune_SkillTooltips_Dir, L"Panel_Lune_SkillTree", L"Lune", luneView), E_FAIL);;
   // CHECK_FAILED(Load_UI(HeroId::Gustave, levelIndex, L"Gustave", Constants::UI::UI_Lune_SkillTree_Path, L"Panel_Gustave_SkillTree", L"Gustave", gustaveView), E_FAIL);;
   // CHECK_FAILED(Load_UI(HeroId::Maelle, levelIndex, L"Maelle", Constants::UI::UI_Lune_SkillTree_Path, L"Panel_Maelle_SkillTree", L"Maelle", maelleView), E_FAIL);;

    _viewByPlayerId.emplace(HeroId::Lune, move(luneView));
    //_viewByPlayerId.emplace(HeroId::Gustave, gustaveView);
    //_viewByPlayerId.emplace(HeroId::Maelle, maelleView);

    _currentView = &_viewByPlayerId[HeroId::Lune];

    Set_SkillPoint();

    _baseSize = _currentView->skillNodes.front().image->Get_Transform()->Get_Scale();
    _hoveredSize = _baseSize * 1.2;
    Close();

    return S_OK;
}

void SkillTree_Panel::Update(float timeDelta)
{
    Vec4 lockedColor = Vec4(0.62f, 0.62f, 0.62f, 1.0f);
    Vec4 redFont = Vec4(186.f, 40.f, 40.f, 255.f);
    Vec4 grayFont = Vec4(108.f, 108.f, 108.f, 255.f);
    Vec4 yellowFont = Vec4(235.f, 207.f, 103.f, 255.f);
    Vec4 defaultFontColor = Vec4(1.f, 1.f, 1.f, 1.f);

    if(!_isOpen)
        return;

    for(auto& [skillId, tooltip] : _currentView->tooltips)
    {
        if(tooltip)
            tooltip->Close();
    }

    for(auto& node : _currentView->skillNodes)
    {
        
        if(node.image)
           node.image->Set_GrayScaleEnabled(false);
            //node.image->Set_Color();

        if(node.costText)
        {
           node.costText->Close();
           node.costText->Set_Color(grayFont);
        }

        if(node.image)
        node.image->Get_Transform()->Set_Scale(_baseSize);

        if(node.lockedPopup)
        node.lockedPopup->Close();

        if(node.unlockedPopup)
        node.unlockedPopup->Close();

        const bool isUnlocked = _currentView->skillTreeCom->IsUnlocked(node.skillId);
        const bool canUnlock = !isUnlocked && _currentView->skillTreeCom->CanUnlock(node.skillId);

        //해금이 되지 않았을 떄
        if(!isUnlocked)
        {
           // //이미지는 회색으로
            if(node.image)
            {
                node.image->Set_GrayScaleEnabled(true);
                node.image->Set_GrayAmount(1);
            }
           //node.image->Set_Color(lockedColor);
            //if(node.costText)
            //node.costText->Set_Color(redFont);
           
            if(!_currentView->skillTreeCom->HasRequiredSkill(node.skillId))
            {
                //잠겨있는 상태 & 선행스킬 없을 때
                //TODO 회색으로 바꾸기
                if(node.costText)
                node.costText->Set_Color(grayFont);  
            }
            else if(_currentView->skillTreeCom->HasRequiredSkill(node.skillId) && !canUnlock )
            {
                //잠겨있는 상태인데 선행스킬은 있는데 포인트가 부족할 때
               //TODO 빨간색으로 바꾸기
                if(node.costText)
                node.costText->Set_Color(redFont);
            }
            else if(_currentView->skillTreeCom->HasRequiredSkill(node.skillId) && canUnlock )
            {
                //잠겨 있는 상태인데 해금 가능한 상태일 때(선행스킬 + 포인트)
                if(node.costText)
                node.costText->Set_Color(yellowFont);
            }
        }

        auto ui = _currentView->tooltips.find(String::ToWString(node.skillId));

        //hover일때
        //TODO 이건 anchor 생기면 다시 보정
        //공통적으로 약간 커짐 && 툴팁 띄우기
        if(node.button->Is_Hovered())
        {
            if(node.image)
            node.image->Get_Transform()->Set_Scale(_hoveredSize);

            //툴팁 띄우기
            if(ui != _currentView->tooltips.end() && ui->second)
            ui->second->Open();

            if(isUnlocked)
            {
                if(node.unlockedPopup)
                node.unlockedPopup->Open();
            }
            else
            {
                if(node.lockedPopup)
                node.lockedPopup->Open();
            }
        }

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
    }
}

void SkillTree_Panel::Open()
{
    _isOpen = true;

    if(_currentView->root)
        _currentView->root->Open();

    for (auto& node : _currentView->skillNodes)
    {
        if (node.unlockedPopup)
            node.unlockedPopup->Close();

        if (node.lockedPopup)
            node.lockedPopup->Close();
    }

    for(auto tooltip : _currentView->tooltips)
        tooltip.second->Close();

    // 마우스 잠금
    GAME->Unlock_Mouse();
    GAME->Set_GameplayPaused(true);
}

void SkillTree_Panel::Close()
{
    _isOpen = false;

    if(_currentView->root)
        _currentView->root->Close();

    for(auto tooltip : _currentView->tooltips)
        tooltip.second->Close();

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

HRESULT SkillTree_Panel::Load_UI(HeroId heroId, uint32 levelIndex, const wstring& playerName,const wstring& skillTreeJsonPath, const wstring& toolTipJsonPath, const wstring& rootName, const wstring& prototypeTagPrefix,SkillTreeViewState& outView)
{
    SkillTreeViewState viewState{};

    UI_RuntimeLoader::UI_LoadDesc desc {};
    desc.levelIndex = levelIndex;
    desc.jsonPath = skillTreeJsonPath;
    desc.prototypeTagPrefix = prototypeTagPrefix + L"SkillTree";
    desc.outUIByName = &outView.uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    outView.root = Find_UI(outView.uiByName, rootName);

    Load_UI_ToolTips(prototypeTagPrefix,toolTipJsonPath, levelIndex, outView);
    Bind_SkillNode_UI(playerName, outView);

    auto hero = CLIENT->Find_Hero(heroId);

    CHECK_NULL(hero, E_FAIL);

    outView.skillTreeCom = hero->Get_Component<SkillTreeCom>();

    CHECK_NULL(outView.skillTreeCom, E_FAIL);

    return S_OK;
}

HRESULT SkillTree_Panel::Load_UI_ToolTips(const wstring& prototypeTagPrefix, const wstring& toolTipJsonPath, uint32 levelIndex, SkillTreeViewState& outView)
{
    UI_RuntimeLoader::UI_LoadDesc desc {};
    desc.levelIndex = levelIndex;
    desc.jsonPath = toolTipJsonPath;
    desc.prototypeTagPrefix = prototypeTagPrefix + L"Tooltip";
    desc.outUIByName = &outView.tooltipsByName;
    desc.outChildrenByParentName = &_childrenByParentName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    for(const auto& [name, ui] : outView.tooltipsByName)
    {
        if(name.rfind(L"Panel_SkillTooltip_Lune", 0) != 0)
            continue;

        const wstring skillId = name.substr(wcslen(L"Panel_SkillTooltip_Lune_"));
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
        const wstring textId = textPrefix + skillIdW;
        const wstring lockedPopupId = L"Panel_Skill_" + playerName + L"_" + skillIdW + L"_Lock";
        const wstring unlockedPopupId =  L"Panel_Skill_" + playerName + L"_" + skillIdW + L"_On"; 

        Shared<UI_Image> icon = dynamic_pointer_cast<UI_Image>(Find_UI(outView.uiByName, imageId));
        Shared<UI_Text>  costText = dynamic_pointer_cast<UI_Text>(Find_UI(outView.uiByName, textId));
        Shared<UI_Base>  lockedPopup = dynamic_pointer_cast<UI_Base>(Find_UI(outView.uiByName, lockedPopupId));
        Shared<UI_Base>  unlockedPopup = dynamic_pointer_cast<UI_Base>(Find_UI(outView.uiByName, unlockedPopupId));

        SkillNodeUI node{};
        node.skillId = skillId;
        node.button = button;
        node.image = icon;
        node.lockedPopup = lockedPopup;
        node.unlockedPopup = unlockedPopup;
        node.costText = costText;

        outView.skillNodes.push_back(node);
    }
    return S_OK;
}

Shared<UI_Base> SkillTree_Panel::Find_UI(const umap<wstring, Shared<UI_Base>>& uiByName, const wstring& name) const
{
    const auto iter = uiByName.find(name);
    if(iter == uiByName.end() || iter->second == nullptr)
        return nullptr;

    return iter->second;
}

void SkillTree_Panel::Set_SkillPoint()
{
    //TODO 플레이어 바뀔 때마다 호출

    for (auto& node : _currentView->skillNodes)
    {
        const auto desc = CLIENT->Get_SkillTreeDesc(node.skillId);

        if(node.costText)
        node.costText->Set_Text(to_wstring(desc->unlockCost));
    }
}

void SkillTree_Panel::Change_ToNextViewState(HeroId next)
{
    uint32 nextIndex = _currentHeroIndex + 1;

    if(nextIndex >= static_cast<uint32>(HeroId::END))
        nextIndex = 0;

    HeroId nextHeroId = static_cast<HeroId>(nextIndex);

    _currentView = &_viewByPlayerId[nextHeroId];
}

HRESULT SkillTree_Panel::Spawn_StatusPreviewHeroes()
{
  //  Clear_StatusPreviewHeroes();
  //
  //  const vector<Shared<Player>> sourceHeroes = CLIENT->Get_HeroesInPartyOrder();
  //
  //  if(sourceHeroes.empty())
  //      return S_OK;
  //
  //  static const string fallbackPrefabs[] =
  //  {
  //      "Player_Lune",
  //      "Player_Maelle",
  //      "Player_Gustave",
  //  };
  //
    return S_OK;
}

void SkillTree_Panel::Clear_StatusPreviewHeroes()
{
}

void SkillTree_Panel::Update_StatusPreviewHeroes(float timeDelta)
{
}

void SkillTree_Panel::Layout_StatusPreviewHeroes()
{
}

HRESULT SkillTree_Panel::Ready_StatusPreviewCamera()
{
    return S_OK;
}

void SkillTree_Panel::Activate_StatusPreviewCamera()
{
}

void SkillTree_Panel::Restore_PreviousCamera()
{
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
    __super::Free();
}
