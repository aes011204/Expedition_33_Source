#pragma once

#include "UI_RuntimePanelBase.h"
#include "Client_Enum.h"

NS_BEGIN(Engine)
class UI_Image;
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_BattleSystemHUD final : public UI_RuntimePanelBase
{
public:
    struct PartyBattleStatSlot
    {
        Shared<UI_Image> hpGauge{};

        Shared<UI_Text> currentHpText{};
        Shared<UI_Text> currentHpTextBg{};
        Shared<UI_Text> maxHpText{};
        Shared<UI_Text> maxHpTextBg{};

        Shared<UI_Text> apText{};
        Shared<UI_Text> apTextBg{};

        Shared<UI_Image> apStates[9]{};
    };

public:
    UI_BattleSystemHUD() = default;
    ~UI_BattleSystemHUD() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

public:
    void    Set_PartyBattleStat(PlayerCharacterType playerType, float curHp, float maxHp, uint32 curAp, uint32 maxAp);
    void    Set_GustaveMechanismCharge(const Shared<BattleMechanismCom>& mechanismCom);
    void    Clear_GustaveMechanismCharge();

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    static uint32 Get_SlotIndex(PlayerCharacterType playerType);
    static void Set_TextPair(const Shared<UI_Text>& front, const Shared<UI_Text>& back, const wstring& text);

private:
    void    Apply_HpGauge(PartyBattleStatSlot& slot, float ratio);
    void    Apply_ApGauge(PartyBattleStatSlot& slot, uint32 curAp, uint32 maxAp);
    static void Set_HpText(PartyBattleStatSlot& slot, float hp, float maxHp);
    static void Set_ApText(PartyBattleStatSlot& slot, uint32 ap);

private:
    PartyBattleStatSlot _partySlots[3]{};

private: /* 구스타브 */
    Shared<UI_Base> _gustaveMechanismRoot{};
    Shared<UI_Text> _gustaveCurrentChargeText{};
    Shared<UI_Text> _gustaveCurrentChargeTextBg{};
    Shared<UI_Text> _gustaveMaxChargeText{};
    Shared<UI_Text> _gustaveMaxChargeTextBg{};

private:
    static constexpr const wchar_t* kPortraitNames[3] =
    {
        L"Gustave",
        L"Lune",
        L"Maelle",
    };

public:
    static Unique<UI_BattleSystemHUD> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
