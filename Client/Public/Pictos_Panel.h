#pragma once

#include "UI_RuntimePanelBase.h"
#include "GameDataTable.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Button;
class UI_Image;
class UI_Text;
NS_END

NS_BEGIN(Client)

class PlayerStatCom;

class Pictos_Panel final : public UI_RuntimePanelBase
{
    GENERATED_BASE_TYPE_BODY(Pictos_Panel)

public:
    Pictos_Panel() = default;
    ~Pictos_Panel() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void    Open() override;
    void    Close() override;
    void    Close_Immediately(bool restoreWorldControl = true);
    void    Close_ForMenuTransition();

    void    Open_AsProgressionContent(HeroId selectedHeroId, size_t selectedSlotIndex);
    void    Close_AsProgressionContent();
    void    Refresh_ForSelectedHero(HeroId selectedHeroId);

    void    Set_SelectedHeroId(HeroId heroId);
    HeroId  Get_SelectedHeroId() const { return _selectedHeroId; }

    void    Set_SelectedEquipSlotIndex(size_t slotIndex);
    size_t  Get_SelectedEquipSlotIndex() const { return _selectedEquipSlotIndex; }

    Shared<UI_Base> Get_RootUI() const { return _root; }
    void    Append_RuntimeUIEntries(
        vector<RuntimeUIEntry>& outEntries,
        const string& category,
        const string& rootName) const override;


private:
    struct EquippedSlotUI
    {
        Shared<UI_Base>   root{};
        Shared<UI_Button> selectButton{};
        Shared<UI_Button> emptyButton{};

        umap<wstring, Shared<UI_Base>> visualUiByName{};
        Shared<UI_Base>   visualRoot{};
        Shared<UI_Button> visualButton{};
        Shared<UI_Image>  visualMarkImage{};
        string            pictosId{};
    };

    struct AvailablePictosRowUI
    {
        umap<wstring, Shared<UI_Base>> uiByName{};
        Shared<UI_Base>   root{};
        Shared<UI_Button> button{};
        string            pictosId{};
        bool              wasHovered = false;
    };

    struct AvailablePictosSlotUI
    {
        Shared<UI_Base>   root{};
        Shared<UI_Button> button{};
        Shared<UI_Text>   nameText{};
        Shared<UI_Base>   markRoot{};
        Shared<UI_Image>  markImage{};
        string            pictosId{};
    };

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Handle_SlotSelectionInput();
    bool    Handle_AvailablePictosInput();
    bool    Handle_ClearEquippedPictosInput();
    void    Clear_EquippedPictos();
    void    Refresh_All();
    void    Refresh_EquippedPictos();
    void    Refresh_AvailablePictos();
    void    Refresh_Tooltips();
    void    Refresh_ActionHints();

    Shared<PlayerStatCom> Resolve_SelectedPlayerStat() const;

    HRESULT Ensure_AvailableRowCount(size_t rowCount);
    HRESULT Ensure_AvailableRowLoaded(size_t rowIndex, const GameDataTable::PictosDesc& pictosDesc);
    HRESULT Ensure_EquippedSlotVisualLoaded(size_t slotIndex, const string& pictosId);
    HRESULT Ensure_TooltipLoaded(const GameDataTable::PictosDesc& pictosDesc);

    void    Clear_AvailableSlot(size_t slotIndex);
    void    Close_BuiltinAvailableRoots();
    void    Clear_EquippedSlotVisual(size_t slotIndex);
    void    Clear_AvailableRow(size_t rowIndex);

    static HeroId Resolve_DefaultHeroId();
    Vec2    Get_AvailableRowPosition(size_t rowIndex) const;

private:
    bool _isProgressionContentMode = false;
    bool _isTooltipHoverArmed = false;
    bool _isClickInputLatched = false; // 같은 마우스 클릭이 UI 재배치 뒤에 다시 소비되지 않게 한 번 떼기 전까지 입력을 막는다.
    HeroId _selectedHeroId = HeroId::END;
    size_t _selectedEquipSlotIndex = 0;
    string _pendingEquipPictosId{}; // Available에서 먼저 고른 픽토스를 저장하고, Equip 슬롯 클릭 때 실제 장착한다.
    Vec2 _tooltipPosition = Vec2(668.f, 255.f); // ToolTip 나오는 위치

    Shared<UI_Base> _availableListRoot{};
    Shared<UI_Base> _selectSlotHintRoot{};
    Shared<UI_Base> _equipHintRoot{};

    array<EquippedSlotUI, 3> _equippedSlots{};
    array<AvailablePictosSlotUI, 6> _availableSlots{};
    vector<AvailablePictosRowUI> _availableRows{};

    array<Shared<UI_Base>, 6> _builtinAvailableRoots{};

    umap<string, umap<wstring, Shared<UI_Base>>> _tooltipUiById{};
    umap<string, Shared<UI_Base>> _tooltipRootsById{};

public:
    static Unique<Pictos_Panel> Create(uint32 levelIndex);
    void Free() override;

};

NS_END
