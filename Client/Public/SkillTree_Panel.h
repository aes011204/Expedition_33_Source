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
        Shared<UI_Text>   costText;
    };

    struct SkillTreeViewState
    {
        Shared<UI_Base>                root;
        Shared<SkillTreeCom>           skillTreeCom;
        vector<SkillNodeUI>            skillNodes;
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

    void   Open();
    void   Close();
    void   Toggle();

    bool   Is_Open() const { return _isOpen;}

private:
    HRESULT     Load_UI(HeroId heroId, uint32 levelIndex,  const wstring& playerName, const wstring& skillTreeJsonPath, const wstring& toolTipJsonPath, const wstring& rootName, const wstring& prototypeTagPrefix,SkillTreeViewState& outView);
    HRESULT     Load_UI_ToolTips(const wstring& prototypeTagPrefix, const wstring& toolTipJsonPath, uint32 levelIndex, SkillTreeViewState& outView);
    HRESULT     Bind_SkillNode_UI(const wstring& playerName, SkillTreeViewState& outView);
    Shared<UI_Base> Find_UI(const umap<wstring, Shared<UI_Base>>& uiByName, const wstring& name) const;

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
    void    Change_ToNextViewState(HeroId next);

private:
    HRESULT Spawn_StatusPreviewHeroes();
    void    Clear_StatusPreviewHeroes();

    void    Update_StatusPreviewHeroes(float timeDelta);
    void    Layout_StatusPreviewHeroes();

    HRESULT Ready_StatusPreviewCamera();
    void    Activate_StatusPreviewCamera();
    void    Restore_PreviousCamera();

private:
    Shared<SkillTreeCom>                     _skillTreeCom;
    uint32                                   _levelIndex = 0;
    bool                                     _isOpen = false;
    umap<wstring, Shared<UI_Base>>           _luneSkillTree;
    umap<wstring, Shared<UI_Base>>           _luneSkillTooltips;
    umap<wstring, vector<Shared<UI_Base>>>   _childrenByParentName;
    vector<SkillNodeUI>                      _skillNodes;
    Shared<UI_Base>                          _root;
    Vec2                                     _baseSize;
    Vec2                                     _hoveredSize;
    umap<HeroId,SkillTreeViewState>          _viewByPlayerId;
    SkillTreeViewState*                      _currentView;
    uint32                                   _currentHeroIndex;

public:
    static Unique<SkillTree_Panel> Create(uint32 levelIndex);
    auto Free() -> void override;
};
NS_END
