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
    _levelIndex = levelIndex;
    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);;

    Close();

    //TODO 선택된 플레이어로 바꿔야 함! 이건 다른 거 같음
    _skillTreeCom = CLIENT->Find_Player(GAME->Current_LevelIndex())->Get_Component<SkillTreeCom>();
//    _baseSize = _skillNodes.front().image->Get_Transform()->Get_Size();
//    _hoveredSize = _baseSize * 1.2f;
    return S_OK;
}

void SkillTree_Panel::Update(float timeDelta)
{
    Vec4 lockedColor =  Vec4(0.35f, 0.35f, 0.35f, 1.0f);
    Vec4 redFont = Vec4(1.f, 0.f,0.f,1.f);
    Vec4 grayFont = Vec4(0.5f, 0.5f, 0.5f, 1.f);
    Vec4 defaultImageColor = Vec4(1.f, 1.f, 1.f, 1.f);
    Vec4 defaultFontColor = Vec4(1.f, 1.f, 1.f, 1.f);

    if(!_isOpen) //_luneSkillTree도 막아야하나?
        return;

    for(auto& [skillId, tooltip] : _luneSkillTooltips)
    {
        if(tooltip)
            tooltip->Close();
    }

    for(auto& node : _skillNodes)
    {
        node.image->Set_Color(defaultImageColor);
        node.costText->Set_Color(defaultFontColor);
        node.image->Get_Transform()->Set_Size(_baseSize);

        //모든 버튼 공통적으로 처리 되는 것
        //기본 상태일 때
        //1. 해금 불가능한 상태일 때
        const bool isUnlocked = _skillTreeCom->IsUnlocked(node.skillId);
        const bool canUnlock = !isUnlocked && _skillTreeCom->CanUnlock(node.skillId);

        if(!isUnlocked)
        {
           // //이미지는 회색으로
            node.image->Set_Color(lockedColor);
           // //글자는 빨간색으로 //선행 스킬은 만족했는데 돈만 부족한 상태일 때에는 빨간색으로
           // node.costText->Set_Color(red);
           
            if(!_skillTreeCom->HasRequiredSkill(node.skillId))
            {
                //잠겨있는 상태 & 선행스킬 없을 때
                //회색으로 바꾸기
               node.costText->Set_Color(grayFont);  
            }
            else
            {
                //잠겨있는 상태인데 선행스킬은 있을 때
               //빨간색으로 바꾸기 
                node.costText->Set_Color(redFont);
            }


            //선행 스킬도 만족하고 돈도 있을 때 상태 복구
        }

        auto ui = _luneSkillTooltips.find(String::ToWString(node.skillId));

        //hover일때
        //공통적으로 약간 커짐 && 툴팁 띄우기
        if(node.button->Is_Hovered())
        {
            node.image->Get_Transform()->Set_Size(_hoveredSize);

            //툴팁 띄우기
            if(ui != _luneSkillTooltips.end() && ui->second)
            ui->second->Open();

            //f 누르면 잠금 해제

        }

        if(node.button->Is_Pressed())
        {
            // 클릭했을 때 모든 버튼 오버레이 처리
            node.image->Set_OverlayEnabled(true);

          //  if(canUnlock)
          //  _skillTreeCom->TryUnlock(   node.skillId);
        }
        else
        {
            node.image->Set_OverlayEnabled(false);
        }
    }
}

void SkillTree_Panel::Open()
{
    _isOpen = true;

    if(_root)
        _root->Open();

    for(auto tooltip : _luneSkillTooltips)
        tooltip.second->Open();
}

void SkillTree_Panel::Close()
{
    _isOpen = false;

    if(_root)
        _root->Close();

    for(auto tooltip : _luneSkillTooltips)
        tooltip.second->Close();
}

void SkillTree_Panel::Toggle()
{
    if(_isOpen)
        Close();
    else
        Open();
}

HRESULT SkillTree_Panel::Load_UI(uint32 levelIndex)
{
    //Lune
    SkillTreeViewState viewState{};

    UI_RuntimeLoader::UI_LoadDesc desc {};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Lune_SkillTree_Path;
    desc.prototypeTagPrefix = L"Lune_SkillTree";
    desc.outUIByName = &_luneSkillTree;
    desc.outChildrenByParentName = &_childrenByParentName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_luneSkillTree, L"Panel_Lune_SkillTree");
    CHECK_NULL(_root, E_FAIL);

    viewState.root = _root;

    CHECK_FAILED(Bind_Lune_SkillNode_UI(),E_FAIL);

    viewState.skillNodes = _skillNodes;
    viewState.skillTreeCom = CLIENT->Find_Player(levelIndex)->Get_Component<SkillTreeCom>();

    CHECK_FAILED(Load_UI_ToolTips(levelIndex), E_FAIL);

    viewState.tooltips = _luneSkillTooltips;

    _viewByPlayerId.emplace("Player_Lune", viewState);

    return S_OK;
}

HRESULT SkillTree_Panel::Load_UI_ToolTips(uint32 levelIndex)
{
    //namespace fs = std::filesystem;
    //
    //const fs::path tooltipDir = fs::path(L"../../../Client/Bin/Resources") / Constants::UI::UI_SkillTooltip_Dir;
    //
    //if (!fs::exists(tooltipDir) || !fs::is_directory(tooltipDir))
    //{
    //    LOG_ERROR("Skill tooltip directory not found: {}", String::ToString(tooltipDir.wstring()));
    //    return E_FAIL;
    //}
    //
    //constexpr wchar_t tooltipFilePrefix[] = L"UI_SkillTooltip_";
    //const size_t prefixLength = wcslen(tooltipFilePrefix);
    //
    //for (const auto& entry : fs::directory_iterator(tooltipDir))
    //{
    //    if (!entry.is_regular_file())
    //        continue;
    //
    //    const fs::path filePath = entry.path();
    //    if (filePath.extension() != L".json")
    //        continue;
    //
    //    const wstring stem = filePath.stem().wstring();
    //    if (stem.rfind(tooltipFilePrefix, 0) != 0)
    //        continue;
    //
    //    const string skillId = String::ToString(stem.substr(prefixLength));
    //
    //    umap<wstring, Shared<UI_Base>> tooltipUiByName;
    //    umap<wstring, vector<Shared<UI_Base>>> tooltipChildrenByParentName;
    //
    //    UI_RuntimeLoader::UI_LoadDesc desc{};
    //    desc.levelIndex = levelIndex;
    //    desc.jsonPath = filePath.wstring();
    //    desc.prototypeTagPrefix = L"Tooltip_" + String::ToWString(skillId);
    //    desc.outUIByName = &tooltipUiByName;
    //    desc.outChildrenByParentName = &tooltipChildrenByParentName;
    //
    //    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);
    //
    //    Shared<UI_Base> tooltipRoot = Find_UI(tooltipUiByName, L"Panel_SkillTooltip");
    //    CHECK_NULL(tooltipRoot, E_FAIL);
    //
    //    tooltipRoot->Close();
    //    _tooltips.emplace(skillId, tooltipRoot);
    //}
    //
    //return S_OK;

    UI_RuntimeLoader::UI_LoadDesc desc {};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Lune_SkillTooltips_Dir;
    desc.prototypeTagPrefix = L"Lune_SkillTooltips";
    desc.outUIByName = &_luneSkillTooltips;
    desc.outChildrenByParentName = &_childrenByParentName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    return S_OK;
}

HRESULT SkillTree_Panel::Bind_Lune_SkillNode_UI()
{
    const wstring buttonPrefix = L"Button_Skill_Lune";
    const wstring imagePrefix = L"Image_Skill_Lune";
    const wstring textPrefix = L"Text_Skill_Lune";

    for(const auto& [name, ui] : _luneSkillTree)
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

        Shared<UI_Image> icon = dynamic_pointer_cast<UI_Image>(Find_UI(_luneSkillTree, imageId));
        Shared<UI_Text> costText = dynamic_pointer_cast<UI_Text>(Find_UI(_luneSkillTree, textId));

        SkillNodeUI node{};
        node.skillId = skillId;
        node.button = button;
        node.image = icon;
        node.costText = costText;

        _skillNodes.push_back(node);
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
