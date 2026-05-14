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
    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Load_UI_ToolTips(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_Lune_SkillNode_UI(), E_FAIL);

    Close();

    _skillTreeCom = CLIENT->Get_ActiveHero()->Get_Component<SkillTreeCom>();

    return S_OK;
}

void SkillTree_Panel::Update(float timeDelta)
{
    if(!_isOpen || !_skillTreeCom)
        return;

    for(auto& [skillId, tooltip] : _tooltips)
    {
        if(tooltip)
            tooltip->Close();
    }

    for(auto& node : _skillNodes)
    {

        //모든 버튼 공통적으로 처리 되는 것
        //기본 상태일 때
        //1. 해금 불가능한 상태일 때
        const bool isUnlocked = _skillTreeCom->IsUnlocked(node.skillId);
        const bool canUnlock = !isUnlocked && _skillTreeCom->CanUnlock(node.skillId);

        if(!isUnlocked)
        {
           // //이미지는 회색으로
           // node.image->Set_Color();
           // //글자는 빨간색으로 //선행 스킬은 만족했는데 돈만 부족한 상태일 때에는 빨간색으로
           // node.costText->Set_Color();
           //
            if(!_skillTreeCom->HasRequiredSkill(node.skillId))
            {
                //잠겨있는 상태 & 선행스킬 없을 때
                //빨간색으로 바꾸기
              //  node.costText->Set_Color();  
            }

        }

        auto ui = _tooltips.find(node.skillId);

        //hover일때
        //공통적으로 약간 커짐 && 툴팁 띄우기
        if(node.button->Is_Hovered())
        {
            if(ui != _tooltips.end() && ui->second)
            ui->second->Open();
        }

        if(node.button->Was_Clicked() && canUnlock)
        {
            _skillTreeCom->TryUnlock(   node.skillId);
        }
        // 클릭했을 때 모든 버튼 오버레이 처리
    }
}

void SkillTree_Panel::Open()
{
    _isOpen = true;

    if(_root)
        _root->Open();
}

void SkillTree_Panel::Close()
{
    _isOpen = false;

    if(_root)
        _root->Close();
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
    UI_RuntimeLoader::UI_LoadDesc desc {};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_SkillTree_Path;
    desc.prototypeTagPrefix = L"SkillTree";
    desc.outUIByName = &_uiByName;
    desc.outChildrenByParentName = &_childrenByParentName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_SkillTree");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT SkillTree_Panel::Load_UI_ToolTips(uint32 levelIndex)
{
    namespace fs = std::filesystem;

    const fs::path tooltipDir = fs::path(L"../../../Client/Bin/Resources") / Constants::UI::UI_SkillTooltip_Dir;

    if (!fs::exists(tooltipDir) || !fs::is_directory(tooltipDir))
    {
        LOG_ERROR("Skill tooltip directory not found: {}", String::ToString(tooltipDir.wstring()));
        return E_FAIL;
    }

    constexpr wchar_t tooltipFilePrefix[] = L"UI_SkillTooltip_";
    const size_t prefixLength = wcslen(tooltipFilePrefix);

    for (const auto& entry : fs::directory_iterator(tooltipDir))
    {
        if (!entry.is_regular_file())
            continue;

        const fs::path filePath = entry.path();
        if (filePath.extension() != L".json")
            continue;

        const wstring stem = filePath.stem().wstring();
        if (stem.rfind(tooltipFilePrefix, 0) != 0)
            continue;

        const string skillId = String::ToString(stem.substr(prefixLength));

        umap<wstring, Shared<UI_Base>> tooltipUiByName;
        umap<wstring, vector<Shared<UI_Base>>> tooltipChildrenByParentName;

        UI_RuntimeLoader::UI_LoadDesc desc{};
        desc.levelIndex = levelIndex;
        desc.jsonPath = filePath.wstring();
        desc.prototypeTagPrefix = L"Tooltip_" + String::ToWString(skillId);
        desc.outUIByName = &tooltipUiByName;
        desc.outChildrenByParentName = &tooltipChildrenByParentName;

        CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

        Shared<UI_Base> tooltipRoot = Find_UI(tooltipUiByName, L"Panel_SkillTooltip");
        CHECK_NULL(tooltipRoot, E_FAIL);

        tooltipRoot->Close();
        _tooltips.emplace(skillId, tooltipRoot);
    }

    return S_OK;
}

HRESULT SkillTree_Panel::Bind_Lune_SkillNode_UI()
{
    const wstring buttonPrefix = L"Button_Skill_Lune";
    const wstring imagePrefix = L"Image_Skill_Lune";
    const wstring textPrefix = L"Text_Skill_Lune";

    for(const auto& [name, ui] : _uiByName)
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

        Shared<UI_Image> icon = dynamic_pointer_cast<UI_Image>(Find_UI(_uiByName, imageId));
        
        Shared<UI_Text> costText = dynamic_pointer_cast<UI_Text>(Find_UI(_uiByName, textId));

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

void SkillTree_Panel::Free()
{
    __super::Free();
}
