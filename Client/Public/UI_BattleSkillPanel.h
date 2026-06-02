#pragma once

#include "UI_RuntimePanelBase.h"
#include "Battle_Enum.h"
#include "Client_Enum.h"

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
    void    Open_Tooltip(const string& skillKey);
    void    Close() override;
    void    Set_EditorDefaultTestOffset(const Vec2& offset) override;

    bool    Supports_EditorHeroTestOffset() const override { return true; }
    HeroId  Get_EditorActiveHeroId() const override { return _activeHeroId; }
    Vec2    Get_EditorHeroTestOffset(HeroId heroId) const override;
    void    Set_EditorHeroTestOffset(HeroId heroId, const Vec2& offset) override;

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Close_MainPanels();
    void    Close_Tooltips();
    HeroId  Resolve_HeroId(PlayerCharacterType playerType) const;
    Vec2    Resolve_HeroTestOffset(HeroId heroId) const;
    void    Apply_CurrentRootEditorOffsets();

private:
    Shared<UI_Base> _gustavePanel{};
    Shared<UI_Base> _lunePanel{};
    Shared<UI_Base> _maellePanel{};

    umap<string, Shared<UI_Base>> _tooltipRoots{};
    HeroId _activeHeroId = HeroId::END;
    Vec2 _gustaveTestOffset = Vec2::Zero;
    Vec2 _luneTestOffset = Vec2::Zero;
    Vec2 _maelleTestOffset = Vec2::Zero;

public:
    static Unique<UI_BattleSkillPanel> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
