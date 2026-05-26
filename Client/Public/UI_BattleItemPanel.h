#pragma once

#include "UI_RuntimePanelBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_BattleItemPanel : public UI_RuntimePanelBase
{
public:
    UI_BattleItemPanel() = default;
    ~UI_BattleItemPanel() override = default;

public:
    HRESULT Initialize(uint32 levelIndex);
    void	Update(float timeDelta) override;


    void Open() override;
    void Close() override;

    void Open_MainPanel(); // 아이템 메인 패널
    void Open_Tooltip(BattleItemType itemType);
    void Close_Tooltip();

    void Refresh_Counts(const Item_Data& healing, const Item_Data& energy, const Item_Data& revive);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

private:
    Shared<UI_Base> _healingTooltipRoot;
    Shared<UI_Base> _energyTooltipRoot;
    Shared<UI_Base> _reviveTooltipRoot;

    Shared<UI_Text> _healingCountText;
    Shared<UI_Text> _energyCountText;
    Shared<UI_Text> _reviveCountText;

public:
    static Unique<UI_BattleItemPanel> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
