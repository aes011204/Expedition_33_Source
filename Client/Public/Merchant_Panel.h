#pragma once

#include <array>

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Button;
class UI_Text;
NS_END

NS_BEGIN(Client)

class Merchant_Panel final : public UI_RuntimePanelBase
{
public:
    Merchant_Panel() = default;
    ~Merchant_Panel() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;
    void    Close_Immediately(bool restoreWorldControl = true);
    void    Close_ForMenuTransition();
    vector<string> Consume_PurchasedPictosIds();

public:
    enum class ShopItemKind
    {
        Weapon,
        Material,
        Pictos
    };

    struct ShopSlotDef
    {
        ShopItemKind kind = ShopItemKind::Material;
        const wchar_t* uiToken = L"";
        const wchar_t* tooltipName = L"";
        const char* characterId = "";
        const char* itemId = "";
        uint32 price = 0;
        uint32 initialStock = 0;
    };

    static constexpr size_t ShopSlotCount = 6;

private:
    struct ShopSlotUI
    {
        Shared<UI_Button> button{};
        Shared<UI_Base>   frame{};
        Shared<UI_Text>   quantityText{};
        Shared<UI_Text>   levelText{};
        Shared<UI_Text>   priceText{};
        Shared<UI_Base>   tooltipRoot{};
        Shared<UI_Base>   pictosMarkFrame{};
        Shared<UI_Base>   pictosMark{};
        Vec4              originalFrameColor{ 1.f, 1.f, 1.f, 1.f };
    };

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Initialize_Stock();
    void    Set_AllUIOpenState(bool isOpen);
    void    Handle_ShopInput();
    void    Refresh_AllSlots();
    void    Refresh_Slot(size_t slotIndex);
    void    Refresh_FrameColor(size_t slotIndex);
    void    Refresh_CurrentChroma();

    bool    Try_Purchase(size_t slotIndex);
    bool    Can_Purchase(size_t slotIndex) const;
    bool    Can_InteractSlot(size_t slotIndex) const;
    bool    Is_SlotOwned(const ShopSlotDef& slot) const;
    uint32  Get_SlotQuantity(size_t slotIndex) const;
    uint32  Get_SlotLevel(const ShopSlotDef& slot) const;
    void    Apply_Purchase(const ShopSlotDef& slot);

public:
    static Unique<Merchant_Panel> Create(uint32 levelIndex);
    void Free() override;

private:
    Shared<UI_Text> _currentChromaText{};
    std::array<ShopSlotUI, ShopSlotCount> _slotUIs{};
    std::array<uint32, ShopSlotCount> _remainingStocks{};
    vector<string> _purchasedPictosIds{};
};

NS_END
