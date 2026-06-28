#include "Merchant_Panel.h"

#include <algorithm>
#include <optional>

#include "Client_Constants.h"
#include "ClientInstance.h"
#include "GameDataTable.h"
#include "GameInstance.h"
#include "PlayerInventory.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr char kChromaCatalystMaterialId[] = "ChromaCatalyst";
    const Vec4 kSoldOutFrameColor{ 180.f / 255.f, 0.f, 0.f, 1.f };

    constexpr std::array<Merchant_Panel::ShopSlotDef, Merchant_Panel::ShopSlotCount> kShopSlots =
    {
        Merchant_Panel::ShopSlotDef{ Merchant_Panel::ShopItemKind::Weapon,   L"Tireso",                  L"Panel_Tireso_Tooltip",                 "Gustave", "Tireso",          158158, 1 },
        Merchant_Panel::ShopSlotDef{ Merchant_Panel::ShopItemKind::Weapon,   L"Plenum",                  L"Panel_Plenum_Tooltip",                 "Maelle",  "Plenum",          158158, 1 },
        Merchant_Panel::ShopSlotDef{ Merchant_Panel::ShopItemKind::Weapon,   L"Snowim",                  L"Panel_Snowim_Tooltip",                 "Lune",    "Snowim",          158158, 1 },
        Merchant_Panel::ShopSlotDef{ Merchant_Panel::ShopItemKind::Material, L"ChromaCatalyst",          L"Panel_ChromaCatalyst_Tooltip",         "",        kChromaCatalystMaterialId, 158,    99 },
        Merchant_Panel::ShopSlotDef{ Merchant_Panel::ShopItemKind::Pictos,   L"Pictos_AugmentedAim",     L"Panel_Pictos_AugmentedAim_Tooltip",    "",        "AugmentedAim",    158158, 1 },
        Merchant_Panel::ShopSlotDef{ Merchant_Panel::ShopItemKind::Pictos,   L"Pictos_AugmentedAttack",  L"Panel_Pictos_AugmentedAttack_Tooltip", "",        "AugmentedAttack", 158158, 1 },
    };

    wstring Format_NumberWithCommas(uint64 value)
    {
        string digits = to_string(value);
        string formatted{};
        formatted.reserve(digits.size() + digits.size() / 3);

        int groupCount = 0;
        for (auto iter = digits.rbegin(); iter != digits.rend(); ++iter)
        {
            if (groupCount == 3)
            {
                formatted.push_back(',');
                groupCount = 0;
            }

            formatted.push_back(*iter);
            ++groupCount;
        }

        reverse(formatted.begin(), formatted.end());
        return String::ToWString(formatted);
    }

    wstring Make_SlotUIName(const wchar_t* prefix, const wchar_t* token, const wchar_t* suffix)
    {
        return wstring(prefix) + token + suffix;
    }
}

HRESULT Merchant_Panel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);
    Initialize_Stock();

    Close_Immediately(false);

    return S_OK;
}

void Merchant_Panel::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_Open())
        return;

    Refresh_AllSlots();
    Handle_ShopInput();
}

void Merchant_Panel::Open()
{
    if (_root == nullptr)
        return;

    if (Is_Open())
        return;

    UI_RuntimePanelBase::Open();
    Set_AllUIOpenState(true);
    Close_AllTooltips();
    Refresh_AllSlots();

    GAME->Play_SoundCue(Constants::SoundCue::UI_Global_GenericEnter);
    GAME->Unlock_Mouse();
    GAME->Set_GameplayInputMode(GameplayInputMode::UIOnly);
}

void Merchant_Panel::Close()
{
    Close_Immediately(true);
}

void Merchant_Panel::Close_Immediately(bool restoreWorldControl)
{
    Close_AllTooltips();
    UI_RuntimePanelBase::Close();
    Set_AllUIOpenState(false);

    if (restoreWorldControl)
    {
        GAME->Set_GameplayInputMode(GameplayInputMode::Normal);
        GAME->Lock_Mouse();
    }
}

void Merchant_Panel::Close_ForMenuTransition()
{
    Close_Immediately(false);
}

HRESULT Merchant_Panel::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Merchant_Path;
    desc.prototypeTagPrefix = L"Merchant";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_Merchant");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT Merchant_Panel::Bind_UI()
{
    _currentChromaText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Chroma", "Merchant");
    CHECK_NULL(_currentChromaText, E_FAIL);

    for (size_t i = 0; i < ShopSlotCount; ++i)
    {
        const ShopSlotDef& slot = kShopSlots[i];
        ShopSlotUI& slotUI = _slotUIs[i];

        slotUI.button = Find_TypedUI<UI_Button>(
            _uiByName,
            Make_SlotUIName(L"Button_Merchant_", slot.uiToken, L""),
            "Merchant");
        slotUI.frame = Find_UI(
            _uiByName,
            Make_SlotUIName(L"Image_Merchant_", slot.uiToken, L"_Frame"));
        slotUI.quantityText = Find_TypedUI<UI_Text>(
            _uiByName,
            Make_SlotUIName(L"Text_Merchant_", slot.uiToken, L"_Current_Quantity"),
            "Merchant");
        slotUI.priceText = Find_TypedUI<UI_Text>(
            _uiByName,
            Make_SlotUIName(L"Text_Merchant_", slot.uiToken, L"_Price"),
            "Merchant");
        slotUI.tooltipRoot = Find_UI(_uiByName, slot.tooltipName);

        CHECK_NULL(slotUI.button, E_FAIL);
        CHECK_NULL(slotUI.frame, E_FAIL);
        CHECK_NULL(slotUI.quantityText, E_FAIL);
        CHECK_NULL(slotUI.priceText, E_FAIL);
        CHECK_NULL(slotUI.tooltipRoot, E_FAIL);

        slotUI.originalFrameColor = slotUI.frame->Get_Color();

        if (slot.kind == ShopItemKind::Weapon)
        {
            slotUI.levelText = Find_TypedUI<UI_Text>(
                _uiByName,
                Make_SlotUIName(L"Text_Merchant_", slot.uiToken, L"_Current_Level_Value"),
                "Merchant");
            CHECK_NULL(slotUI.levelText, E_FAIL);
        }

        if (slot.kind == ShopItemKind::Pictos)
        {
            slotUI.ownedMarkFrame = Find_UI(
                _uiByName,
                Make_SlotUIName(L"Image_Merchant_", slot.uiToken, L"_Mark_Frame"));
            slotUI.ownedMark = Find_UI(
                _uiByName,
                Make_SlotUIName(L"Image_Merchant_", slot.uiToken, L"_Mark"));
            CHECK_NULL(slotUI.ownedMarkFrame, E_FAIL);
            CHECK_NULL(slotUI.ownedMark, E_FAIL);
        }
    }

    return S_OK;
}

void Merchant_Panel::Initialize_Stock()
{
    for (size_t i = 0; i < ShopSlotCount; ++i)
        _remainingStocks[i] = kShopSlots[i].initialStock;
}

void Merchant_Panel::Set_AllUIOpenState(bool isOpen)
{
    for (const auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        if (isOpen)
            ui->Open();
        else
            ui->Close();
    }
}

void Merchant_Panel::Handle_ShopInput()
{
    optional<size_t> hoveredIndex{};

    for (size_t i = 0; i < ShopSlotCount; ++i)
    {
        const ShopSlotUI& slotUI = _slotUIs[i];
        if (slotUI.button == nullptr || !slotUI.button->Is_Open())
            continue;

        if (slotUI.button->Is_Hovered())
            hoveredIndex = i;

        if (slotUI.button->Was_Clicked())
        {
            if (Try_Purchase(i))
                GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_SelectAlly);
            else
                GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);

            Refresh_AllSlots();
            return;
        }
    }

    if (hoveredIndex.has_value())
        Show_Tooltip(*hoveredIndex);
    else
        Close_AllTooltips();
}

void Merchant_Panel::Refresh_AllSlots()
{
    Refresh_CurrentChroma();

    for (size_t i = 0; i < ShopSlotCount; ++i)
        Refresh_Slot(i);
}

void Merchant_Panel::Refresh_Slot(size_t slotIndex)
{
    if (slotIndex >= ShopSlotCount)
        return;

    const ShopSlotDef& slot = kShopSlots[slotIndex];
    ShopSlotUI& slotUI = _slotUIs[slotIndex];

    if (slotUI.quantityText)
        slotUI.quantityText->Set_Text(to_wstring(Get_SlotQuantity(slotIndex)));

    if (slotUI.levelText)
        slotUI.levelText->Set_Text(to_wstring(Get_SlotLevel(slot)));

    if (slotUI.priceText)
        slotUI.priceText->Set_Text(Format_NumberWithCommas(slot.price));

    const bool canInteract = Can_InteractSlot(slotIndex);
    if (slotUI.button)
    {
        slotUI.button->Set_Disabled(!canInteract);
        slotUI.button->Set_AlphaMultiplier(canInteract ? 1.f : 0.45f);
    }

    Refresh_FrameColor(slotIndex);

    if (slot.kind == ShopItemKind::Pictos)
    {
        const bool isOwned = Is_SlotOwned(slot);

        if (slotUI.ownedMarkFrame)
        {
            if (isOwned)
                slotUI.ownedMarkFrame->Open();
            else
                slotUI.ownedMarkFrame->Close();
        }

        if (slotUI.ownedMark)
        {
            if (isOwned)
                slotUI.ownedMark->Open();
            else
                slotUI.ownedMark->Close();
        }
    }
}

void Merchant_Panel::Refresh_FrameColor(size_t slotIndex)
{
    if (slotIndex >= ShopSlotCount)
        return;

    ShopSlotUI& slotUI = _slotUIs[slotIndex];
    if (slotUI.frame == nullptr)
        return;

    slotUI.frame->Set_Color(_remainingStocks[slotIndex] == 0 ? kSoldOutFrameColor : slotUI.originalFrameColor);
}

void Merchant_Panel::Refresh_CurrentChroma()
{
    const Shared<PlayerInventory> inventory = CLIENT->Get_PlayerInventory();
    const uint32 chroma = inventory ? inventory->Get_Chroma() : 0;

    if (_currentChromaText)
        _currentChromaText->Set_Text(Format_NumberWithCommas(chroma));
}

void Merchant_Panel::Close_AllTooltips()
{
    for (ShopSlotUI& slotUI : _slotUIs)
    {
        if (slotUI.tooltipRoot)
            slotUI.tooltipRoot->Close();
    }
}

void Merchant_Panel::Show_Tooltip(size_t slotIndex)
{
    if (slotIndex >= ShopSlotCount)
        return;

    Close_AllTooltips();

    if (_slotUIs[slotIndex].tooltipRoot)
        _slotUIs[slotIndex].tooltipRoot->Open();
}

bool Merchant_Panel::Try_Purchase(size_t slotIndex)
{
    if (slotIndex >= ShopSlotCount)
        return false;

    const ShopSlotDef& slot = kShopSlots[slotIndex];
    if (!Can_Purchase(slotIndex))
        return false;

    const Shared<PlayerInventory> inventory = CLIENT->Get_PlayerInventory();
    if (inventory == nullptr || !inventory->Spend_Chroma(slot.price))
        return false;

    Apply_Purchase(slot);
    if (_remainingStocks[slotIndex] > 0)
        --_remainingStocks[slotIndex];

    LOG_INFO(
        "Merchant_Panel: purchased item={}, price={}",
        slot.itemId,
        slot.price);
    return true;
}

bool Merchant_Panel::Can_Purchase(size_t slotIndex) const
{
    if (slotIndex >= ShopSlotCount)
        return false;

    if (_remainingStocks[slotIndex] == 0)
        return false;

    const ShopSlotDef& slot = kShopSlots[slotIndex];
    const Shared<PlayerInventory> inventory = CLIENT->Get_PlayerInventory();
    if (inventory == nullptr)
        return false;

    if (Is_SlotOwned(slot))
        return false;

    return inventory->Get_Chroma() >= slot.price;
}

bool Merchant_Panel::Can_InteractSlot(size_t slotIndex) const
{
    if (slotIndex >= ShopSlotCount)
        return false;

    if (_remainingStocks[slotIndex] == 0)
        return false;

    const ShopSlotDef& slot = kShopSlots[slotIndex];
    if (CLIENT->Get_PlayerInventory() == nullptr)
        return false;

    return !Is_SlotOwned(slot);
}

bool Merchant_Panel::Is_SlotOwned(const ShopSlotDef& slot) const
{
    const Shared<PlayerInventory> inventory = CLIENT->Get_PlayerInventory();
    if (inventory == nullptr)
        return false;

    switch (slot.kind)
    {
    case ShopItemKind::Weapon:
        return inventory->Is_WeaponUnlocked(slot.characterId, slot.itemId);

    case ShopItemKind::Pictos:
        return inventory->Is_PictosUnlocked(slot.itemId);

    case ShopItemKind::Material:
    default:
        return false;
    }
}

uint32 Merchant_Panel::Get_SlotQuantity(size_t slotIndex) const
{
    if (slotIndex >= ShopSlotCount)
        return 0;

    return _remainingStocks[slotIndex];
}

uint32 Merchant_Panel::Get_SlotLevel(const ShopSlotDef& slot) const
{
    if (slot.kind != ShopItemKind::Weapon)
        return 0;

    const Shared<PlayerInventory> inventory = CLIENT->Get_PlayerInventory();
    if (inventory && inventory->Is_WeaponUnlocked(slot.characterId, slot.itemId))
        return inventory->Get_WeaponEnhanceLevel(slot.characterId, slot.itemId);

    const GameDataTable::WeaponDesc* weapon = CLIENT->Get_WeaponDesc(slot.characterId, slot.itemId);
    return weapon ? weapon->level : 1u;
}

void Merchant_Panel::Apply_Purchase(const ShopSlotDef& slot)
{
    const Shared<PlayerInventory> inventory = CLIENT->Get_PlayerInventory();
    if (inventory == nullptr)
        return;

    switch (slot.kind)
    {
    case ShopItemKind::Weapon:
    {
        const GameDataTable::WeaponDesc* weapon = CLIENT->Get_WeaponDesc(slot.characterId, slot.itemId);
        const uint32 startLevel = weapon ? weapon->level : 1u;
        inventory->Unlock_Weapon(slot.characterId, slot.itemId, startLevel);
        break;
    }

    case ShopItemKind::Material:
        inventory->Add_Material(slot.itemId, 1);
        break;

    case ShopItemKind::Pictos:
        inventory->Unlock_Pictos(slot.itemId);
        break;

    default:
        break;
    }
}

Unique<Merchant_Panel> Merchant_Panel::Create(uint32 levelIndex)
{
    auto instance = make_unique<Merchant_Panel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : Merchant_Panel");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void Merchant_Panel::Free()
{
    if (Is_Open())
        Close_Immediately(false);

    _currentChromaText.reset();

    for (ShopSlotUI& slotUI : _slotUIs)
        slotUI = {};

    UI_RuntimeLoader::Release_Document(_uiByName);
    _root.reset();

    UI_RuntimePanelBase::Free();
}
