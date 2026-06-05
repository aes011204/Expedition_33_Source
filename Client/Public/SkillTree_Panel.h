#pragma once
#include "Base.h"
#include "Client_Defines.h"
#include "Shader_CBuffer_Define.h"
#include "UIPreviewManager.h"

NS_BEGIN(Engine)
    class UI_Base;
class UI_Text;
class UI_Button;
class UI_Image;
class Camera;
NS_END

NS_BEGIN(Client)

class PreviewCamera;
    class SkillTreeCom;
class SkillTree_Panel final : public Base
{
public:
    struct SkillNodeUI
    {
        string            skillId;
        Shared<UI_Button> button;
        Shared<UI_Image>  image;
        Shared<UI_Base>   unlockedPopup;
        Shared<UI_Base>   lockedPopup;
        Shared<UI_Base>   canUnlockPopup;
        Shared<UI_Text>   costText;
        bool              wasHovered = false;
    };

    struct LockNodeUI
    {
        string              name;
        Shared<UI_Button>   button;
        Shared<UI_Image>    image;
        Shared<UI_Base>    lockedPopup;
        bool               wasHovered = false;
    };

    struct SkillTreeViewState
    {
        HeroId                         heroId;
        Shared<UI_Base>                root;
        Shared<UI_Base>                characterSelectUI;
        Shared<SkillTreeCom>           skillTreeCom;
        vector<SkillNodeUI>            skillNodes;
        vector<LockNodeUI>             lockedNodes;
        umap<wstring, Shared<UI_Base>> uiByName;
        Shared<UI_Base>                characterSheet;
        umap<wstring, Shared<UI_Base>> characterSheetUIs;
        umap<wstring, Shared<UI_Base>> tooltipsByName;
        umap<wstring, Shared<UI_Base>> tooltips;
              
        Shared<UI_Text>                levelText{};

        Shared<UI_Text>                vitalityText{};
        Shared<UI_Text>                mightText{};
        Shared<UI_Text>                agilityText{};
        Shared<UI_Text>                defenseStatText{};
        Shared<UI_Text>                luckText{};

        Shared<UI_Text>                curExpText{};
        Shared<UI_Text>                maxExpText{};

        Shared<UI_Text>                curHpText{};
        Shared<UI_Text>                maxHpText{};

        Shared<UI_Text>                hpText{};
        Shared<UI_Text>                attackText{}; 
        Shared<UI_Text>                speedText{};
        Shared<UI_Text>                defenseValueText{};
        Shared<UI_Text>                criticalRateText{};

    };

public:
    SkillTree_Panel();
    virtual ~SkillTree_Panel();

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);

    void    Open();
    void    Close();
    void    Toggle();

    bool    Is_Open() const { return _isOpen;}
    Shared<UI_Base> Get_RootUI() const { return _backgroundUI; }

    void    Handle_SelectionInput();

private:
    HRESULT         Load_Background_UI(uint32 levelIndex);
    HRESULT         Load_UI(HeroId heroId, uint32 levelIndex,  const wstring& playerName, const wstring& skillTreeJsonPath, const wstring& toolTipJsonPath, const wstring& rootName, const wstring& prototypeTagPrefix, const wstring& characterSelectUI,  const wstring& characterJsonPath, SkillTreeViewState& outView);
    HRESULT         Load_CharacterSheet(uint32 levelIndex,const wstring& jsonPath, const wstring& playerName, SkillTreeViewState& outView);
    HRESULT         Load_UI_ToolTips(const wstring& prototypeTagPrefix, const wstring& toolTipJsonPath, uint32 levelIndex, const wstring& playerName, SkillTreeViewState& outView);
    HRESULT         Bind_SkillNode_UI(const wstring& playerName, SkillTreeViewState& outView);
    HRESULT         Bind_LockNode_UI(const wstring& playerName,SkillTreeViewState& outView);
    HRESULT         Bind_Status_UI();
    void            Set_SkillPoint();
    void            Update_UI();

    void            Refresh_CharacterSelectUI();
    void            Refresh_CombatStats();

    static void     Set_IntText(const Shared<UI_Text>& text, int32 value);
    static void     Set_UIntText(const Shared<UI_Text>& text, uint32 value);
    static void     Set_PercentText(const Shared<UI_Text>& text, float value);
    static void     Set_HpText(const Shared<UI_Text>& text, float hp);

private:
    void    Change_ToSelectedViewState();
    void    Apply_PreviewOverrides();           // UI Preview에서 조명과 포스트프로세스 세팅을 SkillTree_Panel용으로 바꿔준다
    void    Restore_PreviewOverrides();         // UI Preview에서 조명과 포스트프로세스 세팅을 원래대로 돌려준다.
    void    Apply_PreviewRimState();            // UI Preview에서 캐릭터에 역광 효과를 입히는 상태로 만든다. 스킬트리 패널에서만 사용한다.
    void    Clear_PreviewRimState();            // UI Preview에서 캐릭터 역광 효과 상태를 해제한다.
    void    Update_PreviewHeroVisibility();


public:
    vector<UIPreviewManager::PreviewSlotDesc> Build_PreviewLayout();

private:
    struct PreviewOverrideState
    {
        bool                        isCaptured = false;
        bool                        hasPrimaryLight = false;
        bool                        hasShadowLight = false;
        bool                        hasPostProcess = false;
        bool                        hasLighting = false;
        Engine::CascadeLevel        cascadeLevel = Engine::CascadeLevel::Normal;
        Engine::LIGHT_DESC          primaryLight{};
        Engine::ShadowLightDesc     shadowLight{};
        Engine::PostProcessCB       postProcess{};
        Engine::PostProcessFeature  postProcessFeatures = Engine::PostProcessFeature::None;
        Engine::DeferredLightingCB  deferredLighting{};
        bool                        iblEnabled = false;
    };

    uint32                                   _selectedHeroIndex = 0;
    bool                                     _isOpen = false;
    Shared<UI_Base>                          _skillpointUI;
    Shared<UI_Base>                          _backgroundUI;
    umap<wstring, Shared<UI_Base>>           _backgroundUIs;
    Shared<SkillTreeCom>                     _skillTreeCom;
    uint32                                   _levelIndex = 0;
    umap<wstring, vector<Shared<UI_Base>>>   _childrenByParentName;
    Vec2                                     _baseSize;
    Vec2                                     _hoveredSize;
    umap<HeroId,SkillTreeViewState>          _viewByPlayerId;
    SkillTreeViewState*                      _currentView;
    PreviewOverrideState                     _previewOverrideState{};




public:
    static Unique<SkillTree_Panel> Create(uint32 levelIndex);
    auto Free() -> void override;
};




NS_END
