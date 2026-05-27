#pragma once

#include "UI_RuntimePanelBase.h"
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

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Close_MainPanels();
    void    Close_Tooltips();

private:
    Shared<UI_Base> _gustavePanel;
    Shared<UI_Base> _lunePanel;
    Shared<UI_Base> _maellePanel;

    umap<string, Shared<UI_Base>> _tooltipRoots;

public:
    static Unique<UI_BattleSkillPanel> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
