#pragma once

#include "UI_RuntimePanelBase.h"
#include "UI_Animation.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

class UI_BattleSkillPanel final : public UI_RuntimePanelBase
{
public:
    UI_BattleSkillPanel() = default;
    ~UI_BattleSkillPanel() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open_MainPanel(PlayerCharacterType playerType);
    void    Open_GradientPanel(PlayerCharacterType playerType);
    void    Open_Tooltip(const string& skillKey);
    void    Close() override;

    void    Close_Immediately();

    void    Set_EditorDefaultTestOffset(const Vec2& offset) override;

    bool    Supports_EditorHeroTestOffset() const override { return true; }
    HeroId  Get_EditorActiveHeroId() const override { return _activeHeroId; }
    Vec2    Get_EditorHeroTestOffset(HeroId heroId) const override;
    void    Set_EditorHeroTestOffset(HeroId heroId, const Vec2& offset) override;

private:
    enum class ActiveView
    {
        None,
        MainPanel,
        GradientPanel,
        Tooltip,
    };

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    HRESULT Load_TooltipAnimations();

    void    Open_MainPanelUIs(HeroId heroId);
    void    Close_MainPanelUIs();
    void    Stop_MainPanelAnimations();
    void    Set_MainPanelSkillOptionsAlpha(HeroId heroId, float alphaMultiplier);
    bool    Play_MainPanelAnimation(HeroId heroId, const wchar_t* animationName);
    vector<Shared<UI_Base>>* Resolve_MainPanelAnimationTargets(HeroId heroId);

    void    Close_MainPanels();
    void    Close_GradientPanels();
    void    Close_Tooltips();
    void    Close_TooltipImmediately();

    HeroId  Resolve_HeroId(PlayerCharacterType playerType) const;
    Vec2    Resolve_HeroTestOffset(HeroId heroId) const;
    void    Apply_CurrentRootEditorOffsets();

private:
    Shared<UI_Base> _gustavePanel{};
    Shared<UI_Base> _lunePanel{};
    Shared<UI_Base> _maellePanel{};
    Shared<UI_Base> _gustaveGradientPanel{};
    Shared<UI_Base> _luneGradientPanel{};
    Shared<UI_Base> _maelleGradientPanel{};
    vector<Shared<UI_Base>> _gustaveMainPanelUIs{};
    vector<Shared<UI_Base>> _luneMainPanelUIs{};
    vector<Shared<UI_Base>> _maelleMainPanelUIs{};

    umap<string, Shared<UI_Base>> _tooltipRoots{};
    HeroId _activeHeroId = HeroId::END;
    Vec2   _gustaveTestOffset = Vec2::Zero;
    Vec2   _luneTestOffset = Vec2::Zero;
    Vec2   _maelleTestOffset = Vec2::Zero;

    ActiveView _activeView = ActiveView::None;

    Shared<UI_Base> _activeTooltipRoot{};

    bool  _isMainPanelOpen = false;
    bool  _isPlayingMainPanelIntro = false;
    float _mainPanelIntroElapsed = 0.f;
    float _mainPanelIntroDuration = 0.f;

    UI_KeyframeAnimationAsset _tooltipIntroAnimation{};
    UI_KeyframeAnimationAsset _tooltipOutroAnimation{};

    bool  _hasTooltipIntroAnimation = false;
    bool  _hasTooltipOutroAnimation = false;

    bool  _isTooltipClosingWithOutro = false;
    float _tooltipCloseElapsed = 0.f;
    float _tooltipOutroDuration = 0.f;

public:
    static Unique<UI_BattleSkillPanel> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
