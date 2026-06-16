#pragma once

#include "GameDataTable.h"
#include "UI_RuntimePanelBase.h"
#include "UIPreviewManager.h"
#include "Shader_CBuffer_Define.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Button;
class UI_Image;
class UI_Text;
NS_END

NS_BEGIN(Client)

class PlayerStatCom;
class SkillTreeCom;

class ChangeSkill_Panel final : public UI_RuntimePanelBase
{
public:
    ChangeSkill_Panel() = default;
    ~ChangeSkill_Panel() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void    Open() override;
    void    Close() override;
    void    Close_Immediately(bool restoreWorldControl = true);
    void    Close_ForMenuTransition();

    void    Open_AsProgressionContent(HeroId selectedHeroId);
    void    Close_AsProgressionContent();
    void    Refresh_ForSelectedHero(HeroId selectedHeroId);

    void    Set_SelectedHeroId(HeroId heroId);
    HeroId  Get_SelectedHeroId() const { return _selectedHeroId; }

    Shared<UI_Base> Get_RootUI() const { return _root; }
    void    Append_RuntimeUIEntries(
        vector<RuntimeUIEntry>& outEntries,
        const string& category,
        const string& rootName) const override;

private:
    struct AvailableSkillSlotUI
    {
        umap<wstring, Shared<UI_Base>> uiByName{};
        Shared<UI_Base>   root{};
        Shared<UI_Button> button{};
        Shared<UI_Image>  frameImage{};
        Shared<UI_Image>  portraitImage{};
        Shared<UI_Text>   nameText{};
        Shared<UI_Text>   apText{};
        Shared<UI_Text>   apBgText{};
        string            skillId{};
        bool              isOwnedByMainDocument = false;
    };

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Handle_ButtonInput();      // 이번 단계는 내부 슬롯 클릭 로그만
    void    Refresh_All();
    void    Refresh_CurrentSkills(const Shared<PlayerStatCom>& statCom);
    void    Refresh_AvailableSkills();

    Shared<PlayerStatCom> Resolve_SelectedPlayerStat() const;
    Shared<SkillTreeCom>  Resolve_SelectedSkillTree() const;
    static HeroId Get_DefaultSelectedHeroId();
    static string Get_CharacterId(HeroId heroId);
    static void Set_Text(const Shared<Engine::UI_Text>& text, const wstring& value);
    void    Apply_PreviewOverrides();
    void    Restore_PreviewOverrides();
    void    Apply_PreviewRimState();
    void    Clear_PreviewRimState();
    void    Update_PreviewHeroVisibility();
    HRESULT Ensure_AvailableSkillSlotCount(size_t slotCount);
    void    Apply_AvailableSkillSlot(size_t slotIndex, const string& skillId, const vector<string>& equippedSkillKeys);
    void    Show_AvailableSkillSlots(bool isVisible);
    static Vec2 Get_AvailableSkillSlotPosition(size_t slotIndex);
    static wstring Resolve_SkillPortraitTexturePath(HeroId heroId, const GameDataTable::SkillTreeDesc* skillTreeDesc);

private:
    static constexpr size_t NormalSkillSlotCount = 5;
    static constexpr size_t InvalidSkillSlotIndex = static_cast<size_t>(-1);

    vector<string> Build_CurrentNormalSkillKeys(const Shared<PlayerStatCom>& statCom) const;
    bool    Try_EquipAvailableSkillToSelectedSlot(const string& skillId);
    void    Refresh_CurrentSkillButtonFocus();

private:
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

private:
    HeroId _selectedHeroId = HeroId::END;
    bool _isProgressionContentMode = false;

    Shared<UI_Base> _root{};
    umap<wstring, Shared<UI_Base>> _uiByName{};

    array<Shared<UI_Button>, 5> _currentSkillButtons{};
    Shared<UI_Button> _gradientButton{};
    Shared<UI_Button> _availableSkillButton{};
    Shared<UI_Button> _skillTreeButton{};

    array<Shared<UI_Text>, 5> _currentSkillNameTexts{};
    array<Shared<UI_Text>, 5> _currentSkillApTexts{};
    array<Shared<UI_Text>, 5> _currentSkillApBgTexts{};
    array<Shared<UI_Image>, 5> _currentSkillIconImages{};
    array<Shared<UI_Base>, 5> _currentSkillEmptyLines{};

    Shared<UI_Text> _gradientText{};
    Shared<UI_Text> _gradientGpText{};
    Shared<UI_Text> _gradientGpBgText{};
    Shared<UI_Image> _gradientIconImage{};
    Shared<UI_Base> _gradientEmptyLine{};

    Shared<UI_Base> _availableSkillListRoot{};
    vector<AvailableSkillSlotUI> _availableSkillSlots{};
    PreviewOverrideState _previewOverrideState{};

    size_t _selectedCurrentSkillSlotIndex = InvalidSkillSlotIndex;

public:
    static Unique<ChangeSkill_Panel> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
