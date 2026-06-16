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

    void    Set_SelectedHeroId(HeroId heroId);
    HeroId  Get_SelectedHeroId() const { return _selectedHeroId; }

    Shared<UI_Base> Get_RootUI() const { return _root; }

private:
    struct AvailableSkillSlotUI
    {
        umap<wstring, Shared<UI_Base>> uiByName{};
        Shared<UI_Base>   root{};
        Shared<UI_Button> button{};
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

    void    Handle_SelectionInput();   // Q/E 입력으로 선택 영웅 변경
    void    Handle_ButtonInput();      // 이번 단계는 내부 슬롯 클릭 로그만
    void    Refresh_All();
    void    Refresh_CharacterSheet(const Shared<PlayerStatCom>& statCom);
    void    Refresh_CombatStats(const Shared<PlayerStatCom>& statCom);
    void    Refresh_CurrentSkills(const Shared<PlayerStatCom>& statCom);
    void    Refresh_AvailableSkills();
    void    Refresh_CharacterSelectUI();

    Shared<PlayerStatCom> Resolve_SelectedPlayerStat() const;
    Shared<SkillTreeCom>  Resolve_SelectedSkillTree() const;
    static HeroId Get_DefaultSelectedHeroId();
    static string Get_CharacterId(HeroId heroId);
    static wstring Get_DisplayHeroName(HeroId heroId);
    static void Set_Text(const Shared<Engine::UI_Text>& text, const wstring& value);
    void    Apply_PreviewOverrides();
    void    Restore_PreviewOverrides();
    void    Apply_PreviewRimState();
    void    Clear_PreviewRimState();
    void    Update_PreviewHeroVisibility();
    HRESULT Ensure_AvailableSkillSlotCount(size_t slotCount);
    void    Apply_AvailableSkillSlot(size_t slotIndex, const string& skillId);
    void    Show_AvailableSkillSlots(bool isVisible);
    static Vec2 Get_AvailableSkillSlotPosition(size_t slotIndex);
    static wstring Resolve_SkillPortraitTexturePath(HeroId heroId, const GameDataTable::SkillTreeDesc* skillTreeDesc);

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

    Shared<UI_Base> _root{};
    umap<wstring, Shared<UI_Base>> _uiByName{};

    array<Shared<UI_Button>, 3> _characterButtons{};
    array<Shared<UI_Image>, 3> _characterStains{};
    array<Shared<UI_Button>, 5> _currentSkillButtons{};
    Shared<UI_Button> _gradientButton{};
    Shared<UI_Button> _availableSkillButton{};
    Shared<UI_Button> _skillTreeButton{};

    Shared<UI_Text> _characterNameText{};
    Shared<UI_Text> _currentLevelText{};
    Shared<UI_Text> _currentExpText{};
    Shared<UI_Text> _maxExpText{};
    Shared<UI_Text> _currentHpText{};
    Shared<UI_Text> _maxHpText{};
    Shared<UI_Text> _currentStatHpText{};
    Shared<UI_Text> _currentStatAttackText{};
    Shared<UI_Text> _currentStatDefenseText{};
    Shared<UI_Text> _currentStatSpeedText{};
    Shared<UI_Text> _currentStatCriticalPercentText{};
    array<Shared<UI_Text>, 5> _currentSkillNameTexts{};
    array<Shared<UI_Text>, 5> _currentSkillApTexts{};
    Shared<UI_Text> _gradientText{};
    Shared<UI_Text> _gradientGpText{};
    Shared<UI_Base> _availableSkillListRoot{};
    vector<AvailableSkillSlotUI> _availableSkillSlots{};
    PreviewOverrideState _previewOverrideState{};

public:
    static Unique<ChangeSkill_Panel> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
