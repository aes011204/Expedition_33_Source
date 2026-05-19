#pragma once

#include "Base.h"
#include "Client_Defines.h"
#include "WeaponUI_Common.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Text;
NS_END

NS_BEGIN(Client)

class ChangeWeapon_Panel final : public Base
{
public:
    ChangeWeapon_Panel() = default;
    ~ChangeWeapon_Panel() override = default;

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);

    void    Open();
    void    Close();
    void    Toggle();

    bool    Is_Open() const { return _isOpen; }

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Refresh_WeaponUI();
    void    Refresh_Tooltip(const GameDataTable::WeaponDesc& weapon);
    void    Refresh_WeaponSelect(const vector<GameDataTable::WeaponDesc>& weapons);

    Shared<UI_Base> Find_UI(
        const umap<wstring, Shared<UI_Base>>& uiByName,
        const wstring& name) const;

    template<typename T>
    Shared<T> Find_TypedUI(
        const umap<wstring, Shared<UI_Base>>& uiByName,
        const wstring& name) const
    {
        Shared<UI_Base> ui = Find_UI(uiByName, name);
        if (ui == nullptr)
            return nullptr;

        Shared<T> typedUI = dynamic_pointer_cast<T>(ui);
        if (typedUI == nullptr)
            LOG_WARN("ChangeWeaponPanel ui type mismatch: {}", String::ToString(name));

        return typedUI;
    }

private:
    static constexpr size_t WeaponSlotCount = 3;

    size_t _selectedWeaponIndex = 0;
    uint32 _levelIndex = 0;
    bool _isOpen = false;

    umap<wstring, Shared<UI_Base>> _uiByName{};
    Shared<UI_Base> _root{};

    WeaponSummaryText _tooltipText{};
    array<WeaponSummaryText, WeaponSlotCount> _weaponSelectTexts{};

public:
    static Unique<ChangeWeapon_Panel> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
