#pragma once

#include "UI_RuntimePanelBase.h"
#include "UIPreviewManager.h"
#include "WeaponUI_Common.h"
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
class UI_AbilityPentagon;

class ChangeWeapon_Panel;
class ChangeSkill_Panel;
class Wardrobe_Panel;
class Pictos_Panel;

class CharacterProgression_Panel : public UI_RuntimePanelBase
{
public:
    enum class Action
    {
        None, OpenChangeWeapon, OpenChangeSkill, OpenWardrobe, OpenPictos, END
    };

    enum class ContentTab
    {
        Overview,
        Weapon,
        Skill,
        Wardrobe,
        Pictos,
    };

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
    };

public:
    CharacterProgression_Panel() = default;
    ~CharacterProgression_Panel() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void    Open() override;
    void    Open(ContentTab initialTab);
    HRESULT Prepare_OpenResources(); // TAB 입력 프레임 전에 생성 가능한 런타임 UI만 선준비한다.
    void    Close() override;

    void    Close_Immediately(bool restoreWorldControl = true);
    void    Close_ForMenuTransition();

    void    Bind_ContentPanels(
        ChangeWeapon_Panel* changeWeaponPanel,
        ChangeSkill_Panel* changeSkillPanel,
        Wardrobe_Panel* wardrobePanel,
		Pictos_Panel* pictosPanel);

    void    Switch_ContentTab(ContentTab nextTab);
    ContentTab Get_ActiveContentTab() const { return _activeContentTab; }

    void    Set_SelectedHeroId(HeroId heroId);
    HeroId  Get_SelectedHeroId() const { return _selectedHeroId; }

    Action  Consume_LastAction();
    Shared<UI_Base> Get_RootUI() const { return _root; }
    void    Append_RuntimeUIEntries(
        vector<RuntimeUIEntry>& outEntries,
        const string& category,
        const string& rootName) const override;

    void    Handle_SelectionInput();
    void    Handle_ButtonInput();

    void    Refresh_All();
    void    Refresh_CharacterSheet(const Shared<PlayerStatCom>& statCom);
    void    Refresh_CombatStats(const Shared<PlayerStatCom>& statCom);
    void    Refresh_CurrentWeapon(const Shared<PlayerStatCom>& statCom);
    void    Refresh_CurrentWeaponThumbnail(const string& weaponId);
    void    Refresh_CurrentSkills(const Shared<PlayerStatCom>& statCom);
	void	Refresh_CurrentPictos(const Shared<PlayerStatCom>& statCom);
    void    Refresh_CharacterSelectUI();
    void    Refresh_UnlockedSkills();

    Shared<PlayerStatCom> Resolve_SelectedPlayerStat() const;
    Shared<SkillTreeCom>  Resolve_SelectedSkillTree() const;

    static HeroId  Get_DefaultSelectedHeroId();
    static wstring Get_DisplayHeroName(HeroId heroId);
    static string  Get_CharacterId(HeroId heroId);
    static void    Set_Text(const Shared<Engine::UI_Text>& text, const wstring& value);

    void    Refresh_AbilityStats(const Shared<PlayerStatCom>& statCom);
    HRESULT Ensure_AbilityPentagonUI();

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Apply_PreviewOverrides();
    void    Restore_PreviewOverrides();
    void    Apply_PreviewRimState();
    void    Clear_PreviewRimState();
    void    Update_PreviewHeroVisibility();

private:
    void    Close_ActiveContentPanel();
    void    Open_ActiveContentPanel();
    void    Refresh_ActiveContentPanel();
    void    Set_OverviewVisible(bool isVisible);

private:
    HRESULT Ensure_AvailableSkillSlotCount(size_t slotCount);
    void    Apply_AvailableSkillSlot(size_t slotIndex, const string& skillId);
    void    Set_SkillAreaVisible(bool isVisible);
    void    Show_EquippedSkillSlots(bool isVisible);
    void    Show_AvailableSkillSlots(bool isVisible);
    static Vec2 Get_AvailableSkillSlotPosition(size_t slotIndex);
    static wstring Resolve_SkillPortraitTexturePath(HeroId heroId, const GameDataTable::SkillTreeDesc* skillTreeDesc);

    pair<PreviewAnimState, PreviewAnimState> Resolve_PreviewAnimationForTab(ContentTab tab) const;
    void Play_SelectedHeroPreviewAnimationForTab(ContentTab tab);

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
	Shared<UI_Base>					_root{};
    umap<wstring, Shared<UI_Base>>	_uiByName{};

    Action _lastAction = Action::None;
    HeroId _selectedHeroId = HeroId::END;
    ContentTab _activeContentTab = ContentTab::Overview;

    // GameplayUI_World가 lifetime을 소유하고, Progression은 현재 탭 콘텐츠 열기/닫기에만 참조한다.
    ChangeWeapon_Panel* _changeWeaponPanel = nullptr;
    ChangeSkill_Panel*  _changeSkillPanel = nullptr;
    Wardrobe_Panel*		_wardrobePanel = nullptr;
	Pictos_Panel*		_picktosPanel = nullptr;
	size_t				_pendingPictosSlotIndex = 0;

    array<Shared<UI_Button>, 3> _characterButtons{};
    array<Shared<UI_Image>, 3> _characterStains{};

    Shared<UI_Base> _abilityPanelRoot{};
    Shared<UI_Image> _abilityPentagonFrame{};
    Shared<UI_Image> _abilityPentagonAnchor{};
    Shared<UI_Text> _currentVitalityText{};
    Shared<UI_Text> _currentMightText{};
    Shared<UI_Text> _currentAgilityText{};
    Shared<UI_Text> _currentDefensePrimaryText{};
    Shared<UI_Text> _currentLuckText{};
    Shared<UI_AbilityPentagon> _abilityPentagonFill{};

    Shared<UI_Base> _currentWeaponPanelRoot{};
    Shared<UI_Base> _currentPictosPanelRoot{};
    Shared<UI_Base> _wardrobePanelRoot{};
    Shared<UI_Base> _tintPanelRoot{};

    Shared<UI_Button> _currentWeaponButton{};
    array<Shared<UI_Button>, 6> _skillButtons{};
    Shared<UI_Button> _skillTreeButton{};
    Shared<UI_Button> _availableSkillButton{};

    Shared<UI_Button> _firstPictosButton{};
    Shared<UI_Button> _secondPictosButton{};
    Shared<UI_Button> _thirdPictosButton{};
    Shared<UI_Button> _wardrobeButton{};
    array<Shared<UI_Text>, 3> _currentPictosNameTexts{};
    array<Shared<UI_Image>, 3> _currentPictosMarkImages{};
    array<Shared<UI_Base>, 3> _currentPictosEmptyImages{};

    Shared<UI_Text> _characterNameText{};
    Shared<UI_Text> _currentLevelText{};
    Shared<UI_Text> _currentExpText{};
    Shared<UI_Text> _maxExpText{};
    Shared<UI_Text> _currentHpText{};
    Shared<UI_Text> _maxHpText{};
    Shared<UI_Image> _expGaugeImage{};
    Shared<UI_Image> _hpGaugeImage{};

    Shared<UI_Text> _currentStatHpText{};
    Shared<UI_Text> _currentStatAttackText{};
    Shared<UI_Text> _currentStatDefenseText{};
    Shared<UI_Text> _currentStatSpeedText{};
    Shared<UI_Text> _currentStatCriticalPercentText{};

    WeaponSummaryText _currentWeaponText{};
    Shared<UI_Base> _colimThumbnailPanel{};
    Shared<UI_Base> _snowimThumbnailPanel{};
    Shared<UI_Base> _baguetteThumbnailPanel{};
    Shared<UI_Base> _lanceramThumbnailPanel{};
    Shared<UI_Base> _candyThumbnailPanel{};
    Shared<UI_Base> _medalumThumbnailPanel{};
    Shared<UI_Base> _sekarumThumbnailPanel{};

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

    PreviewOverrideState _previewOverrideState{};

    Shared<UI_Base> _skillPanelRoot{};
    Shared<UI_Base> _skillTitleRoot{};
    array<Shared<UI_Base>, 6> _equippedSkillSlotRoots{};
    vector<AvailableSkillSlotUI> _availableSkillSlots{};
    bool _isShowingAvailableSkills = false;

public:
    static Unique<CharacterProgression_Panel> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
