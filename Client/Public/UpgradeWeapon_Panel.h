#pragma once

#include <optional>

#include "Base.h"
#include "Client_Defines.h"
#include "GameDataTable.h"
#include "UI_RuntimePanelBase.h"
#include "WeaponUI_Common.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Button;
class UI_Image;
class UI_Text;
NS_END

NS_BEGIN(Client)

class UpgradeWeapon_Panel final : public UI_RuntimePanelBase
{
public:
    UpgradeWeapon_Panel() = default;
    ~UpgradeWeapon_Panel() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta);

    void    Open();
    void    Close();
    void    Close_Immediately(bool restoreWorldControl = true);
    void    Close_ForMenuTransition();

    Shared<UI_Base> Get_RootUI() const { return _root; }
    void    Append_RuntimeUIEntries(
        vector<RuntimeUIEntry>& outEntries,
        const string& category,
        const string& rootName) const override;

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Load_UIFragment(size_t weaponIndex);
    HRESULT Bind_UI();
    HRESULT Bind_FragmentUI(size_t weaponIndex, const umap<wstring, Shared<UI_Base>>& fragmentUIByName);

    void    Handle_WeaponInput();
    void    Handle_UpgradeInput();
    void    Refresh_WeaponRows();
    void    Refresh_SelectedDetails(bool showConfirm);
    void    Refresh_HoverDetails(size_t weaponIndex);
    void    Refresh_Required(size_t weaponIndex, const GameDataTable::WeaponDesc& weapon, uint32 currentLevel);
    void    Apply_DetailUI(
        const WeaponSummaryText& detailUI,
        const GameDataTable::WeaponDesc& weapon,
        const GameDataTable::WeaponUpgradeDesc* upgrade,
        uint32 fallbackLevel,
        uint32 fallbackPower);
    void    Apply_Thumbnail(const Shared<UI_Image>& image, const GameDataTable::WeaponDesc& weapon);
    void    Apply_ThumbnailBackground(const Shared<UI_Image>& image, const string& weaponId);
    void    Apply_EquippedWeaponPower(const string& characterId, const string& weaponId, uint32 power);

    HRESULT Ensure_FragmentLoaded(size_t weaponIndex);
    void    Show_WeaponTooltip(size_t weaponIndex, bool isVisible);
    void    Close_AllWeaponTooltips();
    void    Show_Required(size_t weaponIndex, bool isVisible);
    void    Show_Confirm(size_t weaponIndex, bool isVisible);
    void    Close_AllRequiredPanels();
    void    Close_AllConfirmFragments();
    void    Close_AllDetailFragments();
    void    Apply_BaseDim(bool isDimmed);

public:
    struct WeaponSlotDef
    {
        HeroId heroId = HeroId::END;
        const char* characterId = "";
        const char* weaponId = "";
        const wchar_t* uiSuffix = L"";
        const wchar_t* fragmentToken = L"";
    };

    static constexpr size_t WeaponSlotCount = 9;

private:
    struct WeaponRowUI
    {
        Shared<UI_Button> button{};
        Shared<UI_Image>  icon{};
        Shared<UI_Image>  background{};
        Shared<UI_Text>   weaponNameText{};
        Shared<UI_Text>   levelText{};
    };

    struct WeaponFragmentUI
    {
        bool loaded = false;
        Shared<UI_Base> tooltipRoot{};
        Shared<UI_Base> confirmRoot{};
        WeaponSummaryText hoverDetail{};
        WeaponSummaryText confirmBeforeDetail{};
        WeaponSummaryText confirmAfterDetail{};
        Shared<UI_Image> hoverThumbnail{};
        Shared<UI_Image> hoverThumbnailBackground{};
        Shared<UI_Image> confirmBeforeThumbnail{};
        Shared<UI_Image> confirmBeforeThumbnailBackground{};
        Shared<UI_Image> confirmAfterThumbnail{};
        Shared<UI_Image> confirmAfterThumbnailBackground{};
        Shared<UI_Base> requiredRoot{};
        Shared<UI_Text> currentCatalystText{};
        Shared<UI_Text> requiredCatalystText{};
        Shared<UI_Button> upgradeButton{};
        Shared<UI_Text> upgradeButtonText{};
    };

private:
    uint32 _levelIndex = 0;
    optional<size_t> _selectedWeaponIndex{};

    umap<wstring, Shared<UI_Base>> _uiByName{};
    Shared<UI_Base> _root{};
    Shared<UI_Base> _weaponSelectRoot{};
    Shared<UI_Base> _backgroundRoot{};

    array<WeaponRowUI, WeaponSlotCount> _weaponRows{};
    array<WeaponFragmentUI, WeaponSlotCount> _weaponFragments{};

public:
    static Unique<UpgradeWeapon_Panel> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
