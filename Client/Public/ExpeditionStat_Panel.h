#pragma once

#include "Client_Enum.h"
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Image;
class UI_Text;
NS_END

NS_BEGIN(Client)

class ExpeditionStat_Panel final : public UI_RuntimePanelBase
{
public:
    ExpeditionStat_Panel() = default;
    ~ExpeditionStat_Panel() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void    Open() override;
    void    Close() override;
    void    Toggle();

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();
    void    Refresh_PlayerHpStats();
    void    Set_PlayerHp(HeroId heroId, const wstring& uiName, uint32 slotIndex);
    void    Apply_HpSlot(uint32 slotIndex, float curHp, float maxHp);
    static void Set_Text(const Shared<UI_Text>& text, const wstring& value);

private:
    struct PlayerHpSlot
    {
        Shared<UI_Image> hpGauge{};
        Shared<UI_Text> currentHpText{};
        Shared<UI_Text> maxHpText{};
    };

private:
    static constexpr uint32 kPlayerHpSlotCount = 3;
    PlayerHpSlot _playerHpSlots[kPlayerHpSlotCount]{};

public:
    static Unique<ExpeditionStat_Panel> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
