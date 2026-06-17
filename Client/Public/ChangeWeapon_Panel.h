#pragma once

#include "Base.h"
#include "Client_Defines.h"
#include "Client_Enum.h"
#include "UIPreviewManager.h"
#include "WeaponUI_Common.h"
#include "Shader_CBuffer_Define.h"
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Text;
class UI_Button;
class UI_Image;
NS_END

NS_BEGIN(Client)

class ChangeWeapon_Panel final : public UI_RuntimePanelBase
{
public:
    ChangeWeapon_Panel() = default;
    ~ChangeWeapon_Panel() override = default;

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);

    void    Open();
    void    Close();
    void    Close_Immediately(bool restoreWorldControl = true);
    void    Close_ForMenuTransition();

    void    Open_AsProgressionContent(HeroId selectedHeroId);
    void    Close_AsProgressionContent();
    void    Refresh_ForSelectedHero(HeroId selectedHeroId);

    void    Set_SelectedHeroId(HeroId heroId) { _selectedHeroId = heroId; }
    HeroId  Get_SelectedHeroId() const { return _selectedHeroId; }

    Shared<UI_Base> Get_RootUI() const { return _root; }
    void    Append_RuntimeUIEntries(
        vector<RuntimeUIEntry>& outEntries,
        const string& category,
        const string& rootName) const override;

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

private:
    void    Handle_WeaponSelectionInput(); // 무기 선택했을 때 뜨는 desc

    void    Refresh_WeaponUI();
    void    Refresh_WeaponRows(
                const vector<GameDataTable::WeaponDesc>& weapons,
                const string& equippedWeaponId);
    void    Refresh_Tooltip(const GameDataTable::WeaponDesc& weapon);
    void    Refresh_TooltipDetails(const GameDataTable::WeaponDesc& weapon); // 설명/등급/능력 텍스트를 현재 무기로 교체한다.
    void    Show_WeaponTooltip(bool isVisible);

    void    Apply_PreviewOverrides();
    void    Apply_PreviewRimState();

    void    Restore_PreviewOverrides();
    void    Clear_PreviewRimState();
    void    Update_PreviewHeroVisibility();

private:
    struct WeaponTooltipDetailUI
    {
        Shared<UI_Text> abilityValueText1{};
        Shared<UI_Text> abilityValueText2{};

        Shared<UI_Text> levelTexts[3]{};

        Shared<UI_Text> descriptionTexts[3]{};
    };

    struct WeaponTooltipDetailData
    {
        wstring abilityValue1{};
        wstring abilityValue2{};

        wstring levels[3]{};
        wstring descriptions[3]{};
    };

    struct WeaponRowUI
    {
        Shared<UI_Button> button{};
        Shared<UI_Image>  icon{};
        Shared<UI_Image>  background{};
        Shared<UI_Text>   weaponNameText{};
        Shared<UI_Text>   levelText{};
    };

    struct PreviewOverrideState
    {
        bool                        isCaptured = false;
        bool                        hasPrimaryLight = false;
        bool                        hasShadowLight = false;
        bool                        hasPostProcess = false;
        bool                        hasLighting = false;
        Engine::CascadeLevel        cascadeLevel = Engine::CascadeLevel::Normal;
        Engine::LIGHT_DESC          primaryLight{};
        Engine::ShadowLightDesc     shadowLight{};
        Engine::PostProcessCB       postProcess{};
        Engine::PostProcessFeature  postProcessFeatures = Engine::PostProcessFeature::None;
        Engine::DeferredLightingCB  deferredLighting{};
        bool                        iblEnabled = false;
    };

    static string Get_CharacterId(HeroId heroId); // HeroId -> 무기 데이터 characterId 매핑

private:
    static constexpr size_t WeaponSlotCount = 3;

    size_t _selectedWeaponIndex = 0;
    uint32 _levelIndex = 0;
    HeroId _selectedHeroId = HeroId::END; // Character Progression에서 넘겨준 현재 선택 영웅
    bool _isProgressionContentMode = false;

    umap<wstring, Shared<UI_Base>> _uiByName{};
    Shared<UI_Base> _root{};

    WeaponSummaryText _tooltipText{}; // tooltip 텍스트는 공용 1세트만 갱신한다.
    array<WeaponRowUI, WeaponSlotCount> _weaponRows{}; // authoring 된 row 3개를 그대로 열고 닫는다.
    Shared<UI_Base> _weaponTooltipRoot{};

    PreviewOverrideState _previewOverrideState{};

    WeaponTooltipDetailUI _tooltipDetailUI{};

public:
    static Unique<ChangeWeapon_Panel> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
