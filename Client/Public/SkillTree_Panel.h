#pragma once
#include "Base.h"
#include "Client_Defines.h"

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
    };

    struct LockNodeUI
    {
        string              name;
        Shared<UI_Button>   button;
        Shared<UI_Image>    image;
        Shared<UI_Base>    lockedPopup;    
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
        umap<wstring, Shared<UI_Base>> tooltipsByName;
        umap<wstring, Shared<UI_Base>> tooltips;

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

    void    Handle_SelectionInput();

private:
    HRESULT         Load_Background_UI(uint32 levelIndex);
    HRESULT         Load_UI(HeroId heroId, uint32 levelIndex,  const wstring& playerName, const wstring& skillTreeJsonPath, const wstring& toolTipJsonPath, const wstring& rootName, const wstring& prototypeTagPrefix, const wstring& characterSelectUI, SkillTreeViewState& outView);
    HRESULT         Load_UI_ToolTips(const wstring& prototypeTagPrefix, const wstring& toolTipJsonPath, uint32 levelIndex, const wstring& playerName, SkillTreeViewState& outView);
    HRESULT         Bind_SkillNode_UI(const wstring& playerName, SkillTreeViewState& outView);
    HRESULT         Bind_LockNode_UI(const wstring& playerName,SkillTreeViewState& outView);
    Shared<UI_Base> Find_UI(const umap<wstring, Shared<UI_Base>>& uiByName, const wstring& name) const;
    void            Set_SkillPoint();
    void            Update_UI();

    void            Refresh_CharacterSelectUI();

    template<typename T>
    Shared<T> Find_TypedUI(
        const umap<wstring, Shared<UI_Base>>& uiByName, const wstring& name) const
    {
        Shared<UI_Base> ui = Find_UI(uiByName, name);
        if (ui == nullptr)
            return nullptr;

        Shared<T> typedUI = dynamic_pointer_cast<T>(ui);
        if (typedUI == nullptr)
            LOG_WARN("CharacterStatusPanel ui type mismatch: {}", String::ToString(name));

        return typedUI;
    }
private:
    void    Change_ToSelectedViewState();

private:
    HRESULT Spawn_StatusPreviewHeroes();
    void    Clear_StatusPreviewHeroes();

    void    Update_StatusPreviewHeroes(float timeDelta);
    void    Layout_StatusPreviewHeroes();

    HRESULT Ready_StatusPreviewCamera();
    void    Activate_StatusPreviewCamera();
    void    Restore_PreviousCamera();

private:
    struct StatusPreviewHero
    {
        Shared<Player_WorldBase> player{};

        // 
    };

    uint32                                   _selectedHeroIndex = 0;
    bool                                     _isOpen = false;

    vector<StatusPreviewHero>                _previewHeroes{};
    wstring                                  _previewLayerTag = L"Layer_CharacterStatusPreview";
    Shared<Camera>                           _previousCamera{};
    Shared<PreviewCamera>                    _previewCamera{};
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

public:
    static Unique<SkillTree_Panel> Create(uint32 levelIndex);
    auto Free() -> void override;
};
NS_END
