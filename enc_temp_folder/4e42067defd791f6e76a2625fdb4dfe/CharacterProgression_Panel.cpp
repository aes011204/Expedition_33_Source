#include "CharacterProgression_Panel.h"

#include "Battle_Enum.h"
#include "ChangeSkill_Panel.h"
#include "ChangeWeapon_Panel.h"
#include "Client_Constants.h"
#include "ClientInstance.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "LightingPipeline.h"
#include "Player_WorldBase.h"
#include "PlayerStatCom.h"
#include "PostProcessPipeline.h"
#include "PreviewRimFeature.h"
#include "ResourceLoader.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"
#include "UIPreviewManager.h"
#include "UI_AbilityPentagon.h"
#include "SkillTreeCom.h"
#include "Wardrobe_Panel.h"
#include "Pictos_Panel.h"
#include "GameDataTable.h"

#include "PreviewPlayer.h"

using namespace UI_Utils;

namespace
{
    constexpr std::array<HeroId, 3> kHeroOrder =
    {
        HeroId::Lune,
        HeroId::Gustave,
        HeroId::Maelle,
    };

    void Set_UIMapOpenState(const umap<wstring, Shared<UI_Base>>& uiByName, bool isOpen)
    {
        for (const auto& [uiName, ui] : uiByName)
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

    // Overview일 때 마엘만 반대로 회전되게
    vector<UIPreviewManager::PreviewSlotDesc> Build_CharacterProgressionPreviewLayout(
        CharacterProgression_Panel::ContentTab tab)
    {
        const Vec3 maelleRotation =
            (tab == CharacterProgression_Panel::ContentTab::Overview)
                ? Vec3(0.f, -90.f, -0.1f)
                : Vec3(0.f, 0.f, -0.1f);

        return
        {
            { HeroId::Lune,    Vec3(1.1f,  0.f, -0.1f), Vec3(0.f, 0.f, -0.1f) },
            { HeroId::Gustave, Vec3(0.0f,  0.f,  0.f), Vec3(0.f, 0.f,  0.f) },
            { HeroId::Maelle,  Vec3(-1.1f, 0.f, -0.1f), maelleRotation },
        };
    }

    void Show_CharacterProgressionPreview(HeroId heroId, CharacterProgression_Panel::ContentTab tab)
    {
        const auto previewLayout = Build_CharacterProgressionPreviewLayout(tab);

        PreviewShotPanel panel = PreviewShotPanel::CharacterProgression;

        switch(tab)
        {
        case CharacterProgression_Panel::ContentTab::Pictos:
            panel = PreviewShotPanel::CharacterProgression;
            break;

        case CharacterProgression_Panel::ContentTab::Skill:
            panel =PreviewShotPanel::ChangeSkill;
            break;

        case CharacterProgression_Panel::ContentTab::Wardrobe:
            panel = PreviewShotPanel::Wardrobe;
            break;

        case CharacterProgression_Panel::ContentTab::Weapon:
            panel = PreviewShotPanel::ChangeWeapon;
            break;

        case CharacterProgression_Panel::ContentTab::Overview:
        default:
            panel = PreviewShotPanel::CharacterProgression;
            break;
        }

        CLIENT->Apply_PreviewLayout(previewLayout);
        CLIENT->Show_Preview(heroId, panel);
    }
}

HRESULT CharacterProgression_Panel::Initialize(uint32 levelIndex)
{
    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately(false);

    return S_OK;
}

void CharacterProgression_Panel::Update(float timeDelta)
{
    if (!Is_Open())
        return;

    Handle_SelectionInput(); // Q/E 입력으로 현재 보는 영웅을 바꾼다.
    Handle_ButtonInput();

    if (_activeContentTab != ContentTab::Overview)
    {
        const Shared<PlayerStatCom> statCom = Resolve_SelectedPlayerStat();
        if (statCom)
        {
            Refresh_CharacterSheet(statCom);
            Refresh_CombatStats(statCom);
            Refresh_AbilityStats(statCom); // 내부 탭에서도 장비/픽토스 변경 스탯을 즉시 표시한다.
        }
    }
}

void CharacterProgression_Panel::Open()
{
    Open(ContentTab::Overview);
}

HRESULT CharacterProgression_Panel::Prepare_OpenResources()
{
    if (_abilityPentagonFill != nullptr)
        return S_OK;

    // preview 캐릭터와 달리 UI 안에서만 쓰는 작은 런타임 도형이라 미리 만들어도 메뉴 흐름을 바꾸지 않는다.
    return Ensure_AbilityPentagonUI();
}

void CharacterProgression_Panel::Open(ContentTab initialTab)
{
    if (_root == nullptr)
        return;

    if (Is_Open())
        return;

    UI_RuntimePanelBase::Open();
    Close_ActiveContentPanel(); // 이전 TAB 종료 때 남은 내부 탭 UI가 있으면 새 루트 진입 전에 정리한다.
    Set_UIMapOpenState(_uiByName, true);

    if (_selectedHeroId == HeroId::END)
        _selectedHeroId = Get_DefaultSelectedHeroId();

    if (FAILED(Prepare_OpenResources()))
        LOG_WARN("CharacterProgression_Panel: failed to prepare runtime open resources");

    _lastAction = Action::None;
    _activeContentTab = ContentTab::Overview;

    CLIENT->Begin_Preview(PreviewShotPanel::CharacterProgression);
    Show_CharacterProgressionPreview(_selectedHeroId, _activeContentTab);
    Update_PreviewHeroVisibility();
    Apply_PreviewOverrides();

    if (initialTab != ContentTab::Overview)
    {
        Switch_ContentTab(initialTab);
    }
    else
    {
        Play_SelectedHeroPreviewAnimationForTab(_activeContentTab);
    }

    Refresh_All();

    GAME->Play_SoundCue(Constants::SoundCue::UI_Global_GenericEnter);
    GAME->Unlock_Mouse();
    GAME->Set_GameplayPaused(true); // CharacterStatus/SkillTree처럼 preview 패널 동안 월드 진행을 멈춘다.
}

void CharacterProgression_Panel::Close()
{
    Close_Immediately(true);
}

void CharacterProgression_Panel::Close_Immediately(bool restoreWorldControl)
{
    _lastAction = Action::None;
    Close_ActiveContentPanel();
    _activeContentTab = ContentTab::Overview;
    _isShowingAvailableSkills = false;
    Set_SkillAreaVisible(false); // 닫는 중에는 Progression의 기술 영역을 다시 열지 않는다.
    UI_RuntimePanelBase::Close();
    Set_UIMapOpenState(_uiByName, false);
    Restore_PreviewOverrides();
    CLIENT->Hide_Preview();
    if (restoreWorldControl)
    {
        GAME->Set_GameplayPaused(false);
        GAME->Lock_Mouse();
    }
}

void CharacterProgression_Panel::Close_ForMenuTransition()
{
    Close_Immediately(false);
}

void CharacterProgression_Panel::Bind_ContentPanels(
    ChangeWeapon_Panel* changeWeaponPanel,
    ChangeSkill_Panel* changeSkillPanel,
    Wardrobe_Panel* wardrobePanel,
    Pictos_Panel* pictosPanel)
{
    _changeWeaponPanel = changeWeaponPanel;
    _changeSkillPanel = changeSkillPanel;
    _wardrobePanel = wardrobePanel;
    _picktosPanel = pictosPanel;
}

void CharacterProgression_Panel::Switch_ContentTab(ContentTab nextTab)
{
    if (_activeContentTab == nextTab)
        return;

    _lastAction = Action::None;

    Close_ActiveContentPanel();
    _activeContentTab = nextTab;

    Show_CharacterProgressionPreview(_selectedHeroId, _activeContentTab);
    Update_PreviewHeroVisibility();
    Play_SelectedHeroPreviewAnimationForTab(_activeContentTab);

    if (_activeContentTab == ContentTab::Overview)
    {
        Set_OverviewVisible(true);
        Refresh_All();
        return;
    }

    Set_OverviewVisible(false);
    Open_ActiveContentPanel();
}

void CharacterProgression_Panel::Close_ActiveContentPanel()
{
    if (_changeWeaponPanel)
        _changeWeaponPanel->Close_AsProgressionContent();

    if (_changeSkillPanel)
        _changeSkillPanel->Close_AsProgressionContent();

    if (_wardrobePanel)
        _wardrobePanel->Close_AsProgressionContent();

    if (_picktosPanel)
        _picktosPanel->Close_AsProgressionContent();
}

void CharacterProgression_Panel::Open_ActiveContentPanel()
{
    const HeroId heroId = (_selectedHeroId == HeroId::END) ? Get_DefaultSelectedHeroId() : _selectedHeroId;

    switch (_activeContentTab)
    {
    case ContentTab::Weapon:
        if (_changeWeaponPanel)
            _changeWeaponPanel->Open_AsProgressionContent(heroId);
        break;

    case ContentTab::Skill:
        if (_changeSkillPanel)
            _changeSkillPanel->Open_AsProgressionContent(heroId);
        break;

    case ContentTab::Wardrobe:
        if (_wardrobePanel)
            _wardrobePanel->Open_AsProgressionContent(heroId);
        break;

    case ContentTab::Pictos:
        if (_picktosPanel)
            _picktosPanel->Open_AsProgressionContent(heroId, _pendingPictosSlotIndex); // Overview에서 누른 슬롯 번호를 그대로 넘겨 같은 장착 칸을 편집한다.
        break;

    case ContentTab::Overview:
    default:
        break;
    }
}

void CharacterProgression_Panel::Refresh_ActiveContentPanel()
{
    const HeroId heroId = (_selectedHeroId == HeroId::END) ? Get_DefaultSelectedHeroId() : _selectedHeroId;

    switch (_activeContentTab)
    {
    case ContentTab::Weapon:
        if (_changeWeaponPanel)
            _changeWeaponPanel->Refresh_ForSelectedHero(heroId);
        break;

    case ContentTab::Skill:
        if (_changeSkillPanel)
            _changeSkillPanel->Refresh_ForSelectedHero(heroId);
        break;

    case ContentTab::Wardrobe:
        if (_wardrobePanel)
            _wardrobePanel->Refresh_ForSelectedHero(heroId);
        break;

    case ContentTab::Pictos:
        if (_picktosPanel)
        {
            _picktosPanel->Set_SelectedEquipSlotIndex(_pendingPictosSlotIndex); // Character Progression 쪽 현재 선택 슬롯을 Pictos 패널에도 동기화한다.
            _picktosPanel->Refresh_ForSelectedHero(heroId);
        }
        break;

    case ContentTab::Overview:
    default:
        break;
    }
}

void CharacterProgression_Panel::Set_OverviewVisible(bool isVisible)
{
    if (_currentWeaponPanelRoot)
    {
        if (isVisible)
            _currentWeaponPanelRoot->Open();
        else
            _currentWeaponPanelRoot->Close();
    }

    if (_currentPictosPanelRoot)
    {
        if (isVisible)
            _currentPictosPanelRoot->Open();
        else
            _currentPictosPanelRoot->Close();
    }

    if (_wardrobePanelRoot)
    {
        if (isVisible)
            _wardrobePanelRoot->Open();
        else
            _wardrobePanelRoot->Close();
    }

    if (_tintPanelRoot)
    {
        if (isVisible)
            _tintPanelRoot->Open();
        else
            _tintPanelRoot->Close();
    }

    if (isVisible)
    {
        Set_SkillAreaVisible(true);
        Show_EquippedSkillSlots(true);
    }
    else
    {
        Set_SkillAreaVisible(false);
    }
}

void CharacterProgression_Panel::Set_SelectedHeroId(HeroId heroId)
{
    _selectedHeroId = (heroId == HeroId::END) ? Get_DefaultSelectedHeroId() : heroId;

    if (Is_Open())
    {
        Show_CharacterProgressionPreview(_selectedHeroId, _activeContentTab);
        Update_PreviewHeroVisibility();
        Apply_PreviewOverrides();
        if (_activeContentTab == ContentTab::Weapon)
            Play_SelectedHeroPreviewAnimationForTab(_activeContentTab); // Weapon 탭은 캐릭터 전환 때도 무기 장착 Intro를 다시 보여준다.
        else
            Play_SelectedHeroProgressionIdle();
        Refresh_All();

        if (_activeContentTab != ContentTab::Overview)
            Refresh_ActiveContentPanel();
    }
}

CharacterProgression_Panel::Action CharacterProgression_Panel::Consume_LastAction()
{
    const Action action = _lastAction;
    _lastAction = Action::None;

    return action;
}

void CharacterProgression_Panel::Append_RuntimeUIEntries(
    vector<RuntimeUIEntry>& outEntries,
    const string& category,
    const string& rootName) const
{
    if (_root == nullptr)
        return;

    RuntimeUIEntry rootEntry{};
    rootEntry.category = category;
    rootEntry.name = rootName;
    rootEntry.ui = _root;
    rootEntry.editorTarget = const_cast<CharacterProgression_Panel*>(this);
    outEntries.push_back(rootEntry);

    Append_RuntimeUIMapEntries(outEntries, category, _uiByName, _root, const_cast<CharacterProgression_Panel*>(this));
}

void CharacterProgression_Panel::Handle_SelectionInput()
{
    const vector<HeroPartyEntry>& partyEntries = CLIENT->Get_HeroEntries();
    if (partyEntries.empty())
        return;

    HeroId previousHeroId = _selectedHeroId;
    if (previousHeroId == HeroId::END)
        previousHeroId = partyEntries.front().heroId;

    if (GAME->KeyDown(KEY_TYPE::Q) || GAME->KeyDown(KEY_TYPE::E))
    {
        uint32 currentIndex = 0;
        for (uint32 i = 0; i < static_cast<uint32>(partyEntries.size()); ++i)
        {
            if (partyEntries[i].heroId == previousHeroId)
            {
                currentIndex = i;
                break;
            }
        }

        if (GAME->KeyDown(KEY_TYPE::Q))
            currentIndex = (currentIndex + 1) % static_cast<uint32>(partyEntries.size());
        else
            currentIndex = (currentIndex + static_cast<uint32>(partyEntries.size()) - 1) % static_cast<uint32>(partyEntries.size());


        _selectedHeroId = partyEntries[currentIndex].heroId;
    }

    if (_selectedHeroId == HeroId::END)
        _selectedHeroId = previousHeroId;

    if (_selectedHeroId != previousHeroId)
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_SelectAlly);
        Show_CharacterProgressionPreview(_selectedHeroId, _activeContentTab); // Q/E로 선택 영웅이 바뀌면 preview target도 즉시 교체한다.
        Update_PreviewHeroVisibility();
        Apply_PreviewOverrides();

        if (_activeContentTab == ContentTab::Weapon)
            Play_SelectedHeroPreviewAnimationForTab(_activeContentTab); // Weapon 탭은 Q/E 전환 때 Weapon_Intro -> Weapon_Idle을 유지한다.
        else
            Play_SelectedHeroProgressionIdle();

        Refresh_All();

        if (_activeContentTab != ContentTab::Overview)
            Refresh_ActiveContentPanel();
    }
}

void CharacterProgression_Panel::Handle_ButtonInput()
{
    if (_activeContentTab != ContentTab::Overview)
    {
        return;
    }

    // 장착 무기
    if (_currentWeaponButton && _currentWeaponButton->Was_Clicked())
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);
        _lastAction = Action::OpenChangeWeapon;
        return;
    }

    // 스킬
    for (const Shared<UI_Button>& skillButton : _skillButtons)
    {
        if (skillButton && skillButton->Was_Clicked())
        {
            GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);
            _lastAction = Action::OpenChangeSkill;
            return;
        }
    }

	if (_availableSkillButton && _availableSkillButton->Was_Clicked())
	{
		GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);
		_lastAction = Action::OpenChangeSkill;
		return;
	}

	if (_skillTreeButton && _skillTreeButton->Was_Clicked())
	{
		GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);
		_lastAction = Action::OpenChangeSkill;
		return;
	}

    // 옷장
    if (_wardrobeButton && _wardrobeButton->Was_Clicked())
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);
        _lastAction = Action::OpenWardrobe;
        return;
    }


    // 픽토스
    if ((_firstPictosButton && _firstPictosButton->Was_Clicked()) ||
        (_secondPictosButton && _secondPictosButton->Was_Clicked()) ||
        (_thirdPictosButton && _thirdPictosButton->Was_Clicked()))
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);
        if (_firstPictosButton && _firstPictosButton->Was_Clicked())
            _pendingPictosSlotIndex = 0; // 첫 번째 픽토스 슬롯 편집 진입
        else if (_secondPictosButton && _secondPictosButton->Was_Clicked())
            _pendingPictosSlotIndex = 1; // 두 번째 픽토스 슬롯 편집 진입
        else
            _pendingPictosSlotIndex = 2; // 세 번째 픽토스 슬롯 편집 진입

        _lastAction = Action::OpenPictos;
        return;
    }
}

void CharacterProgression_Panel::Refresh_All()
{
    const Shared<PlayerStatCom> statCom = Resolve_SelectedPlayerStat();

    Refresh_CharacterSelectUI();

    if (statCom == nullptr)
    {
        Set_Text(_characterNameText, Get_DisplayHeroName(_selectedHeroId));
        Set_Text(_currentLevelText, L"0");
        Set_Text(_currentExpText, L"0");
        Set_Text(_maxExpText, L"0");
        Set_Text(_currentHpText, L"0");
        Set_Text(_maxHpText, L"0");

        Set_Text(_currentStatHpText, L"0");
        Set_Text(_currentStatAttackText, L"0");
        Set_Text(_currentStatDefenseText, L"0");
        Set_Text(_currentStatSpeedText, L"0");
        Set_Text(_currentStatCriticalPercentText, L"0");

        if (_activeContentTab == ContentTab::Overview)
        {
            _isShowingAvailableSkills = false;
            Set_OverviewVisible(true);
            Set_Text(_currentWeaponText.weaponNameText, L"-");
            Set_Text(_currentWeaponText.levelText, L"0");
            Refresh_CurrentWeaponThumbnail("");
            Refresh_CurrentSkills(nullptr);
            Refresh_CurrentPictos(nullptr);
        }
        else
        {
            Set_OverviewVisible(false);
        }

        if (_expGaugeImage)
            _expGaugeImage->Set_FillRatio(0.f);

        if (_hpGaugeImage)
            _hpGaugeImage->Set_FillRatio(0.f);
        return;
    }

    const HeroId heroId = (_selectedHeroId == HeroId::END) ? Get_DefaultSelectedHeroId() : _selectedHeroId;
    CLIENT->Ensure_DefaultEquippedWeapon(heroId); // 전투 스탯 표시 전에 기본 무기 공격력을 먼저 확정한다.

    Refresh_CharacterSheet(statCom);
    Refresh_CombatStats(statCom);
    Refresh_AbilityStats(statCom);

    if (_activeContentTab == ContentTab::Overview)
    {
        _isShowingAvailableSkills = false;
        Set_OverviewVisible(true);
        Refresh_CurrentWeapon(statCom);
        Refresh_CurrentSkills(statCom);
        Refresh_CurrentPictos(statCom);
    }
    else
    {
        Set_OverviewVisible(false);
    }
}

void CharacterProgression_Panel::Refresh_CharacterSheet(const Shared<PlayerStatCom>& statCom)
{
    if (statCom == nullptr)
        return;

    const PlayerStatCom::PRuntimeCombatStat& statInfo = statCom->Get_PlayerRuntimeStat();
    const uint32 maxExp = statInfo.CurMaxExp > 0 ? statInfo.CurMaxExp : max(1u, statInfo.nextExp);
    const float expRatio = std::clamp(
        maxExp > 0 ? static_cast<float>(statInfo.CurExp) / static_cast<float>(maxExp) : 0.f,
        0.f,
        1.f);
    const float hpRatio = std::clamp(
        statInfo.CurMaxHp > 0 ? static_cast<float>(statInfo.CurHp) / static_cast<float>(statInfo.CurMaxHp) : 0.f,
        0.f,
        1.f);

    Set_Text(_characterNameText, Get_DisplayHeroName(_selectedHeroId));
    Set_UIntText(_currentLevelText, statInfo.CurLevel);
    Set_UIntText(_currentExpText, statInfo.CurExp);
    Set_UIntText(_maxExpText, maxExp);
    Set_IntText(_currentHpText, static_cast<int32>(statInfo.CurHp));
    Set_IntText(_maxHpText, static_cast<int32>(statInfo.CurMaxHp));

    if (_expGaugeImage)
        _expGaugeImage->Set_FillRatio(expRatio); // 현재 경험치 / 다음 레벨 필요 경험치 비율

    if (_hpGaugeImage)
        _hpGaugeImage->Set_FillRatio(hpRatio);   // 현재 HP / 최대 HP 비율
}

void CharacterProgression_Panel::Refresh_CombatStats(const Shared<PlayerStatCom>& statCom)
{
    if (statCom == nullptr)
        return;

    const PlayerStatCom::PRuntimeCombatStat& statInfo = statCom->Get_PlayerRuntimeStat();

    Set_IntText(_currentStatHpText, static_cast<int32>(statInfo.CurMaxHp));
    Set_IntText(_currentStatAttackText, static_cast<int32>(statInfo.CurPhysicalAttack));
    Set_IntText(_currentStatDefenseText, static_cast<int32>(statInfo.CurPhysicalDefense));
    Set_IntText(_currentStatSpeedText, static_cast<int32>(statInfo.CurSpeed));
    Set_IntText(_currentStatCriticalPercentText, static_cast<int32>(statInfo.CurCriticalRate));
}

void CharacterProgression_Panel::Refresh_CurrentWeapon(const Shared<PlayerStatCom>& statCom)
{
    if (statCom == nullptr)
        return;

    const HeroId heroId = (_selectedHeroId == HeroId::END) ? Get_DefaultSelectedHeroId() : _selectedHeroId;
    CLIENT->Ensure_DefaultEquippedWeapon(heroId); // 기본 디폴트 무기 확정

    const string characterId = Get_CharacterId(heroId);
    const string equippedWeaponId = statCom->Get_PlayerRuntimeStat().CurEquippedWeaponId;
    const GameDataTable::WeaponDesc* equippedWeapon = CLIENT->Get_WeaponDesc(characterId, equippedWeaponId);

    if (equippedWeapon != nullptr)
    {
        Apply_WeaponSummary(_currentWeaponText, *equippedWeapon);
        Refresh_CurrentWeaponThumbnail(equippedWeapon->weaponId); // 장착 무기 썸네일만 연다.
        return;
    }

    const vector<GameDataTable::WeaponDesc>* weapons = CLIENT->Get_WeaponDescs(characterId);
    if (weapons != nullptr && !weapons->empty())
    {
        Apply_WeaponSummary(_currentWeaponText, weapons->front());
        Refresh_CurrentWeaponThumbnail(weapons->front().weaponId); // 장착 무기 ID를 못 찾으면 첫 무기 썸네일로 fallback 한다.
        return;
    }

    Refresh_CurrentWeaponThumbnail("");
    Set_Text(_currentWeaponText.weaponNameText, L"-");
    Set_Text(_currentWeaponText.levelText, L"0");
}

void CharacterProgression_Panel::Refresh_CurrentWeaponThumbnail(const string& weaponId)
{
    const auto closeAll = [this]()
        {
            if (_colimThumbnailPanel) _colimThumbnailPanel->Close();
            if (_snowimThumbnailPanel) _snowimThumbnailPanel->Close();
            if (_baguetteThumbnailPanel) _baguetteThumbnailPanel->Close();
            if (_lanceramThumbnailPanel) _lanceramThumbnailPanel->Close();
            if (_candyThumbnailPanel) _candyThumbnailPanel->Close();
            if (_medalumThumbnailPanel) _medalumThumbnailPanel->Close();
            if (_sekarumThumbnailPanel) _sekarumThumbnailPanel->Close();
        };

    closeAll();

    if (weaponId == "Colim")
    {
        if (_colimThumbnailPanel) _colimThumbnailPanel->Open();
    }
    else if (weaponId == "Esquim")
    {
        if (_snowimThumbnailPanel) _snowimThumbnailPanel->Open(); // 현재 authoring 이름은 Snowim panel을 쓴다.
    }
    else if (weaponId == "Baguette")
    {
        if (_baguetteThumbnailPanel) _baguetteThumbnailPanel->Open();
    }
    else if (weaponId == "Lanceram")
    {
        if (_lanceramThumbnailPanel) _lanceramThumbnailPanel->Open();
    }
    else if (weaponId == "Candy")
    {
        if (_candyThumbnailPanel) _candyThumbnailPanel->Open();
    }
    else if (weaponId == "Medalum")
    {
        if (_medalumThumbnailPanel) _medalumThumbnailPanel->Open();
    }
    else if (weaponId == "Sekarum")
    {
        if (_sekarumThumbnailPanel) _sekarumThumbnailPanel->Open();
    }
}

void CharacterProgression_Panel::Refresh_CurrentSkills(const Shared<PlayerStatCom>& statCom)
{
    for (size_t i = 0; i < _currentSkillNameTexts.size(); ++i)
    {
        Set_Text(_currentSkillNameTexts[i], L"-");
        Set_Text(_currentSkillApTexts[i], L"0");
        Set_Text(_currentSkillApBgTexts[i], L"0");

        if (_currentSkillIconImages[i])
            _currentSkillIconImages[i]->Close(); // 장착 스킬이 없는 슬롯은 Json 기본 이미지 숨기기

        if (_currentSkillEmptyLines[i])
            _currentSkillEmptyLines[i]->Open(); // 빈 슬롯일 때만 JSON의 '-' 표시
    }

    Set_Text(_gradientText, L"-");
    Set_Text(_gradientGpText, L"0");
    Set_Text(_gradientGpBgText, L"0");

    if (_gradientIconImage)
        _gradientIconImage->Close();

    if (_gradientEmptyLine)
        _gradientEmptyLine->Open();

    if (statCom == nullptr)
        return;

    vector<string> skillKeys = statCom->Get_SkillKeys();
    if (skillKeys.empty())
    {
        const Shared<SkillTreeCom> skillTreeCom = Resolve_SelectedSkillTree();
        if (skillTreeCom != nullptr && !skillTreeCom->Get_UnlockedSkillIds().empty())
            skillKeys = skillTreeCom->Get_UnlockedSkillIds(); // 장착값이 아직 없으면 해금 스킬 순서를 초기 5칸 표시 기준으로 쓴다.
    }

    if (skillKeys.size() > 5)
        skillKeys.resize(5); // 6번째 Gradient 슬롯은 일반 스킬 표시에서 제외한다.
    else
        skillKeys.resize(5);

    for (size_t i = 0; i < skillKeys.size(); ++i)
    {
        const string& skillKey = skillKeys[i];
        if (skillKey.empty())
            continue;

        const GameDataTable::SkillTreeDesc* skillTreeDesc = CLIENT->Get_SkillTreeDesc(skillKey);
        const Skill_Data* skillData = CLIENT->Get_Skill(skillKey);

        Set_Text(_currentSkillNameTexts[i], skillTreeDesc ? skillTreeDesc->skillName : String::ToWString(skillKey));
        Set_IntText(_currentSkillApTexts[i], skillData ? skillData->apCost : 0);
        Set_IntText(_currentSkillApBgTexts[i], skillData ? skillData->apCost : 0);

        const wstring portraitPath = Resolve_SkillPortraitTexturePath(_selectedHeroId, skillTreeDesc);
        if (_currentSkillIconImages[i] && !portraitPath.empty())
        {
            _currentSkillIconImages[i]->Set_TexturePath(portraitPath);
            _currentSkillIconImages[i]->Open();
        }
        else if (_currentSkillIconImages[i])
        {
            _currentSkillIconImages[i]->Close();
        }

        if (_currentSkillEmptyLines[i])
            _currentSkillEmptyLines[i]->Close(); // 스킬 이름이 들어간 슬롯에서는 앞 '-' 작대기를 숨긴다.
    }

    // 6번째 슬롯은 일반 스킬 overflow가 아니라 추후 Gradient 스킬 전용 슬롯으로 비워 둔다.
}

void CharacterProgression_Panel::Refresh_CurrentPictos(const Shared<PlayerStatCom>& statCom)
{
    for (size_t slotIndex = 0; slotIndex < _currentPictosNameTexts.size(); ++slotIndex)
    {
        if (_currentPictosNameTexts[slotIndex])
        {
            _currentPictosNameTexts[slotIndex]->Set_Text(L"");
            _currentPictosNameTexts[slotIndex]->Close();
        }

        if (_currentPictosMarkImages[slotIndex])
            _currentPictosMarkImages[slotIndex]->Close(); // 장착된 슬롯만 체크 마크를 보여준다.

        if (_currentPictosEmptyImages[slotIndex])
            _currentPictosEmptyImages[slotIndex]->Open(); // 비어 있는 슬롯은 authoring된 empty 이미지를 그대로 쓴다.
    }

    if (statCom == nullptr)
        return;

    const array<string, PlayerStatCom::EquippedPictosSlotCount>& equippedPictosIds = statCom->Get_EquippedPictosIds();
    for (size_t slotIndex = 0; slotIndex < equippedPictosIds.size(); ++slotIndex)
    {
        const string& pictosId = equippedPictosIds[slotIndex];
        if (pictosId.empty())
            continue;

        wstring pictosName = String::ToWString(pictosId);
        if (const auto* pictosDesc = CLIENT->Get_PictosDesc(pictosId))
            pictosName = pictosDesc->name;

        if (_currentPictosNameTexts[slotIndex])
        {
            _currentPictosNameTexts[slotIndex]->Set_Text(pictosName); // 장착된 슬롯만 실제 이름을 넣는다.
            _currentPictosNameTexts[slotIndex]->Open();               // 이름을 넣은 슬롯만 다시 노출한다.
        }

        if (_currentPictosMarkImages[slotIndex])
            _currentPictosMarkImages[slotIndex]->Open();

        if (_currentPictosEmptyImages[slotIndex])
            _currentPictosEmptyImages[slotIndex]->Close();
    }
}

void CharacterProgression_Panel::Refresh_CharacterSelectUI()
{
    for (size_t i = 0; i < kHeroOrder.size(); ++i)
    {
        if (_characterStains[i] == nullptr)
            continue;

        if (kHeroOrder[i] == _selectedHeroId)
            _characterStains[i]->Open();  // 현재 선택 영웅
        else
            _characterStains[i]->Close();
    }
}

void CharacterProgression_Panel::Refresh_UnlockedSkills()
{
    const Shared<SkillTreeCom> skillTreeCom = Resolve_SelectedSkillTree();
    if (skillTreeCom == nullptr)
    {
        Show_EquippedSkillSlots(false);
        Show_AvailableSkillSlots(false);
        return;
    }

    const vector<string>& unlockedSkillIds = skillTreeCom->Get_UnlockedSkillIds();
    if (FAILED(Ensure_AvailableSkillSlotCount(unlockedSkillIds.size())))
        return;

    Show_EquippedSkillSlots(false);

    for (size_t i = 0; i < _availableSkillSlots.size(); ++i)
    {
        AvailableSkillSlotUI& slot = _availableSkillSlots[i];

        if (i >= unlockedSkillIds.size())
        {
            if (slot.root)
                slot.root->Close();
            continue;
        }

        Apply_AvailableSkillSlot(i, unlockedSkillIds[i]);

        if (slot.root)
            slot.root->Open();
    }
}

Shared<PlayerStatCom> CharacterProgression_Panel::Resolve_SelectedPlayerStat() const
{
    const HeroId heroId = (_selectedHeroId == HeroId::END) ? Get_DefaultSelectedHeroId() : _selectedHeroId;

    const Shared<Player_WorldBase> hero = CLIENT->Find_Hero(heroId);
    if (hero == nullptr)
        return nullptr;

    return hero->Get_Component<PlayerStatCom>();
}

Shared<SkillTreeCom> CharacterProgression_Panel::Resolve_SelectedSkillTree() const
{
    const HeroId heroId = (_selectedHeroId == HeroId::END) ? Get_DefaultSelectedHeroId() : _selectedHeroId;

    const Shared<Player_WorldBase> hero = CLIENT->Find_Hero(heroId);
    if (hero == nullptr)
        return nullptr;

    return hero->Get_Component<SkillTreeCom>();
}

HeroId CharacterProgression_Panel::Get_DefaultSelectedHeroId()
{
    const vector<HeroPartyEntry>& partyEntries = CLIENT->Get_HeroEntries();
    if (!partyEntries.empty())
        return partyEntries.front().heroId;

    return HeroId::Gustave;
}

wstring CharacterProgression_Panel::Get_DisplayHeroName(HeroId heroId)
{
    switch (heroId)
    {
    case HeroId::Gustave:
        return L"구스타브";
    case HeroId::Lune:
        return L"루네";
    case HeroId::Maelle:
        return L"마엘";
    case HeroId::END:
    default:
        return L"-";
    }
}

string CharacterProgression_Panel::Get_CharacterId(HeroId heroId)
{
    switch (heroId)
    {
    case HeroId::Gustave:
        return "Gustave";
    case HeroId::Lune:
        return "Lune";
    case HeroId::Maelle:
        return "Maelle";
    case HeroId::END:
    default:
        return "Gustave";
    }
}

void CharacterProgression_Panel::Set_Text(const Shared<UI_Text>& text, const wstring& value)
{
    if (text)
        text->Set_Text(value);
}

void CharacterProgression_Panel::Refresh_AbilityStats(const Shared<PlayerStatCom>& statCom)
{
    if (statCom == nullptr)
        return;

    const PlayerStatCom::PRuntimeCombatStat& statInfo = statCom->Get_PlayerRuntimeStat();

    Set_IntText(_currentVitalityText, statInfo.CurVitality);
    Set_IntText(_currentMightText, statInfo.CurMight);
    Set_IntText(_currentAgilityText, statInfo.CurAgility);
    Set_IntText(_currentDefensePrimaryText, statInfo.CurDefense);
    Set_IntText(_currentLuckText, statInfo.CurLuck);

    if (_abilityPentagonFill)
    {
        _abilityPentagonFill->Set_StatValues(
        {
            statInfo.CurVitality,
            statInfo.CurMight,
            statInfo.CurAgility,
            statInfo.CurDefense,
            statInfo.CurLuck,
        });
    }
}

HRESULT CharacterProgression_Panel::Ensure_AbilityPentagonUI()
{
    if (_abilityPentagonFill)
        return S_OK;

    CHECK_NULL(_abilityPanelRoot, E_FAIL);
    CHECK_NULL(_abilityPentagonFrame, E_FAIL);
    CHECK_NULL(_abilityPentagonAnchor, E_FAIL);

    static const wstring kAbilityPentagonRuntimeTag = L"CharacterProgression_AbilityPentagon";

    if (!GAME->Has_Prototype(_levelIndex, kAbilityPentagonRuntimeTag))
        CHECK_FAILED(GAME->Add_Prototype(_levelIndex, kAbilityPentagonRuntimeTag, UI_AbilityPentagon::Create()), E_FAIL);

    UI_AbilityPentagon::UI_ABILITY_PENTAGON_DESC desc{};

    if (UI_Transform* transform = _abilityPentagonFrame->Get_Transform())
    {
        desc.transformDesc.position = transform->Get_Position();
        desc.transformDesc.size = transform->Get_Size();
        desc.transformDesc.scale = transform->Get_Scale();
        desc.transformDesc.pivot = transform->Get_Pivot();
        desc.transformDesc.anchorMin = transform->Get_AnchorMin();
        desc.transformDesc.anchorMax = transform->Get_AnchorMax();
        desc.transformDesc.rotationRadians = transform->Get_RotationRadians();
    }

    desc.isOpenOnCreate = true;
    desc.sortOrder = _abilityPentagonAnchor->Get_SortOrder();
    desc.renderGroup = _abilityPentagonAnchor->Get_RenderGroup();
    desc.texturePath = ResourceLoader::Resolve_RuntimePath(L"../Bin/Resources/Textures/UI/Common/Utility/T_White.png");
    desc.color = _abilityPentagonAnchor->Get_Color();
    desc.maxStatValue = 99.f;     // 일단은 임시로 최대 99 기준. 스탯 올라가는 양 보고 결정하기
    desc.baseInnerRatio = 0.18f;  // 0에서도 중심 pentagon 유지

    Shared<UI_Base> createdBase{};
    CHECK_FAILED(GAME->Add_UI(_levelIndex, kAbilityPentagonRuntimeTag, &desc, &createdBase), E_FAIL);

    _abilityPentagonFill = dynamic_pointer_cast<UI_AbilityPentagon>(createdBase);
    CHECK_NULL(_abilityPentagonFill, E_FAIL);

    _abilityPentagonFill->Set_Parent(_abilityPanelRoot);
    _abilityPentagonAnchor->Close(); // 기존 json의 작은 중심 pentagon은 숨긴다.

    return S_OK;
}

void CharacterProgression_Panel::Apply_PreviewOverrides()
{
    const Shared<PostProcessPipeline> postProcessPipeline = GAME->Get_PostProcessPipeline();
    const Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();
    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    const LIGHT_DESC* primaryLightDesc = GAME->Get_PrimaryShadowLightDesc();
    const ShadowLightDesc* shadowLightDesc = GAME->Get_ShadowLightDesc();
    if (primaryLightDesc == nullptr)
    {
        const uint32 lightCount = GAME->Get_LightCount();
        for (uint32 lightIndex = 0; lightIndex < lightCount; ++lightIndex)
        {
            const LIGHT_DESC* lightDesc = GAME->Get_LightDesc(lightIndex);
            if (lightDesc == nullptr || lightDesc->type != LightType::Directional)
                continue;

            primaryLightDesc = lightDesc;
            break;
        }
    }

    if (previewManager == nullptr)
        return;

    if (_previewOverrideState.isCaptured == false)
    {
        _previewOverrideState.isCaptured = true;
        _previewOverrideState.cascadeLevel = GAME->Get_ShadowCascadeLevel();
        _previewOverrideState.hasPrimaryLight = (primaryLightDesc != nullptr);
        _previewOverrideState.hasShadowLight = (shadowLightDesc != nullptr);
        _previewOverrideState.hasPostProcess = (postProcessPipeline != nullptr);
        _previewOverrideState.hasLighting = (lightingPipeline != nullptr);

        if (primaryLightDesc != nullptr)
            _previewOverrideState.primaryLight = *primaryLightDesc;

        if (shadowLightDesc != nullptr)
            _previewOverrideState.shadowLight = *shadowLightDesc;

        if (postProcessPipeline != nullptr)
        {
            _previewOverrideState.postProcess = postProcessPipeline->Get_PostProcessParams();
            _previewOverrideState.postProcessFeatures = postProcessPipeline->Get_PostProcessFeatures();
        }

        if (lightingPipeline != nullptr)
        {
            _previewOverrideState.deferredLighting = lightingPipeline->Get_DeferredLightingParams();
            _previewOverrideState.iblEnabled = lightingPipeline->Is_IBLEnabled();
        }
    }

    if (_previewOverrideState.hasPrimaryLight)
    {
        LIGHT_DESC previewLight = _previewOverrideState.primaryLight;
        Vec3 previewLightDir = kPreviewLightDirection;
        if (previewLightDir.LengthSquared() > 0.0001f)
            previewLightDir.Normalize();

        previewLight.direction = Vec4(previewLightDir.x, previewLightDir.y, previewLightDir.z, 0.f);
        previewLight.diffuse = kPreviewLightDiffuse;
        previewLight.ambient = kPreviewLightAmbient;
        previewLight.specular = kPreviewLightSpecular;
        GAME->Update_PrimaryShadowLightDesc(previewLight);
        GAME->Set_ShadowCascadeLevel(Engine::CascadeLevel::Close);
    }

    if (_previewOverrideState.hasShadowLight)
    {
        ShadowLightDesc previewShadow = _previewOverrideState.shadowLight;
        previewShadow.intensity = max(previewShadow.intensity, kPreviewShadowIntensity);
        GAME->Update_ShadowLight(previewShadow);
    }

    if (_previewOverrideState.hasPostProcess && postProcessPipeline != nullptr)
    {
        Engine::PostProcessCB previewPostProcess = _previewOverrideState.postProcess;
        previewPostProcess.exposure = 0.60f;
        previewPostProcess.gamma = 1.33f;
        previewPostProcess.bloomThreshold = 0.78f;
        previewPostProcess.bloomIntensity = 0.64f;
        previewPostProcess.bloomScatter = 0.88f;
        postProcessPipeline->Set_PostProcessFeatures(
            Engine::PostProcessFeature::HDR |
            Engine::PostProcessFeature::ToneMapping |
            Engine::PostProcessFeature::Exposure |
            Engine::PostProcessFeature::Bloom |
            Engine::PostProcessFeature::Gamma);
        postProcessPipeline->Set_PostProcessParams(previewPostProcess);
    }

    if (_previewOverrideState.hasLighting && lightingPipeline != nullptr)
    {
        Engine::DeferredLightingCB previewDeferredLighting = _previewOverrideState.deferredLighting;
        previewDeferredLighting.iblIntensity = 0.64f;
        previewDeferredLighting.iblDiffuseIntensity = 0.34f;
        previewDeferredLighting.iblSpecularIntensity = 1.18f;
        lightingPipeline->Set_DeferredLightingParams(previewDeferredLighting);
        lightingPipeline->Set_IBLEnabled(true);
    }

    Apply_PreviewRimState();
}

void CharacterProgression_Panel::Restore_PreviewOverrides()
{
    if (!_previewOverrideState.isCaptured)
        return;

    Clear_PreviewRimState();

    const Shared<PostProcessPipeline> postProcessPipeline = GAME->Get_PostProcessPipeline();
    const Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();

    if (_previewOverrideState.hasPrimaryLight)
    {
        GAME->Update_PrimaryShadowLightDesc(_previewOverrideState.primaryLight);
        GAME->Set_ShadowCascadeLevel(_previewOverrideState.cascadeLevel);
    }

    if (_previewOverrideState.hasShadowLight)
        GAME->Update_ShadowLight(_previewOverrideState.shadowLight);

    if (_previewOverrideState.hasPostProcess && postProcessPipeline != nullptr)
    {
        postProcessPipeline->Set_PostProcessFeatures(_previewOverrideState.postProcessFeatures);
        postProcessPipeline->Set_PostProcessParams(_previewOverrideState.postProcess);
    }

    if (_previewOverrideState.hasLighting && lightingPipeline != nullptr)
    {
        lightingPipeline->Set_DeferredLightingParams(_previewOverrideState.deferredLighting);
        lightingPipeline->Set_IBLEnabled(_previewOverrideState.iblEnabled);
    }

    _previewOverrideState = PreviewOverrideState{};
}

void CharacterProgression_Panel::Apply_PreviewRimState()
{
    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    if (previewManager == nullptr)
        return;

    for (const auto& previewHero : previewManager->Get_PreviewHeroes())
    {
        if (previewHero.player == nullptr)
            continue;

        const Shared<EffectCom> effectCom = previewHero.player->Get_Component<EffectCom>();
        if (effectCom == nullptr)
            continue;

        if (previewHero.player->Is_Visible())
            effectCom->Set_PreviewRim(
                kPreviewRimColor,
                kPreviewRimIntensity,
                kPreviewRimPower,
                kPreviewHairRimIntensity);
        else
            effectCom->Clear_PreviewRim();
    }
}

void CharacterProgression_Panel::Clear_PreviewRimState()
{
    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    if (previewManager == nullptr)
        return;

    for (const auto& previewHero : previewManager->Get_PreviewHeroes())
    {
        if (previewHero.player == nullptr)
            continue;

        if (const Shared<EffectCom> effectCom = previewHero.player->Get_Component<EffectCom>())
            effectCom->Clear_PreviewRim();
    }
}

void CharacterProgression_Panel::Update_PreviewHeroVisibility()
{
    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    if (previewManager == nullptr)
        return;

    for (const auto& previewHero : previewManager->Get_PreviewHeroes())
    {
        if (previewHero.player == nullptr)
            continue;

        previewHero.player->Set_Visible(previewHero.heroId == _selectedHeroId);
    }
}

HRESULT CharacterProgression_Panel::Ensure_AvailableSkillSlotCount(size_t slotCount)
{
    CHECK_NULL(_skillPanelRoot, E_FAIL);

    while (_availableSkillSlots.size() < slotCount)
    {
        const size_t slotIndex = _availableSkillSlots.size();

        AvailableSkillSlotUI slot{};

        UI_RuntimeLoader::UI_LoadDesc desc{};
        desc.levelIndex = _levelIndex;
        desc.jsonPath = Constants::UI::UI_Available_Skill_Path;
        desc.prototypeTagPrefix = L"CharacterProgressionAvailableSkill_" + to_wstring(slotIndex);
        desc.outUIByName = &slot.uiByName;

        CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

        slot.root = Find_UI(slot.uiByName, L"Panel_Available_Skill");
        slot.button = Find_TypedUI<UI_Button>(slot.uiByName, L"Button_Available_Skill", "CharacterProgressionAvailableSkill");
        slot.portraitImage = Find_TypedUI<UI_Image>(slot.uiByName, L"Image_Available_Skill", "CharacterProgressionAvailableSkill");
        slot.nameText = Find_TypedUI<UI_Text>(slot.uiByName, L"Text_Available_Skill", "CharacterProgressionAvailableSkill");
        slot.apText = Find_TypedUI<UI_Text>(slot.uiByName, L"Text_Available_Skill_AP", "CharacterProgressionAvailableSkill");
        slot.apBgText = Find_TypedUI<UI_Text>(slot.uiByName, L"Text_Available_Skill_AP_BG", "CharacterProgressionAvailableSkill");

        CHECK_NULL(slot.root, E_FAIL);
        CHECK_NULL(slot.button, E_FAIL);
        CHECK_NULL(slot.portraitImage, E_FAIL);
        CHECK_NULL(slot.nameText, E_FAIL);
        CHECK_NULL(slot.apText, E_FAIL);
        CHECK_NULL(slot.apBgText, E_FAIL);

        slot.root->Set_Parent(_skillPanelRoot); // Available Skill 카드 JSON을 Character Progression의 Skill 영역 안에 붙인다.
        if (slot.root->Get_Transform())
        {
            slot.root->Get_Transform()->Set_Position(Get_AvailableSkillSlotPosition(slotIndex));
            slot.root->Refresh_OpenBaseTransform(); // Open()이 standalone JSON 원래 위치(800,450)로 되돌리지 않게 한다.
        }

        slot.root->Close();
        _availableSkillSlots.push_back(slot);
    }

    return S_OK;
}

void CharacterProgression_Panel::Apply_AvailableSkillSlot(size_t slotIndex, const string& skillId)
{
    if (slotIndex >= _availableSkillSlots.size())
        return;

    AvailableSkillSlotUI& slot = _availableSkillSlots[slotIndex];
    slot.skillId = skillId;

    if (slot.root && slot.root->Get_Transform())
    {
        slot.root->Get_Transform()->Set_Position(Get_AvailableSkillSlotPosition(slotIndex)); // 해금 순서대로 좌/우 2열 배치
        slot.root->Refresh_OpenBaseTransform();
    }

    const GameDataTable::SkillTreeDesc* skillTreeDesc = CLIENT->Get_SkillTreeDesc(skillId);
    const Skill_Data* skillData = CLIENT->Get_Skill(skillId);

    Set_Text(slot.nameText, skillTreeDesc ? skillTreeDesc->skillName : String::ToWString(skillId));

    const wstring apText = to_wstring(skillData ? skillData->apCost : 0);
    Set_Text(slot.apText, apText);
    Set_Text(slot.apBgText, apText);

    const wstring portraitPath = Resolve_SkillPortraitTexturePath(_selectedHeroId, skillTreeDesc);
    if (slot.portraitImage && !portraitPath.empty())
        slot.portraitImage->Set_TexturePath(portraitPath); // DT_SkillTree portrait 값만 카드 썸네일에 교체한다.
}

void CharacterProgression_Panel::Set_SkillAreaVisible(bool isVisible)
{
    if (_skillPanelRoot)
    {
        if (isVisible)
            _skillPanelRoot->Open();
        else
            _skillPanelRoot->Close();
    }

    if (_skillTitleRoot)
    {
        if (isVisible)
            _skillTitleRoot->Open();
        else
            _skillTitleRoot->Close();
    }

    if (!isVisible)
    {
        Show_AvailableSkillSlots(false);
        Show_EquippedSkillSlots(false);
        return;
    }

    Show_AvailableSkillSlots(false); // 기본 Progression 화면은 장착 스킬만 보여준다.
}

void CharacterProgression_Panel::Show_EquippedSkillSlots(bool isVisible)
{
	for (const auto& slotRoot : _equippedSkillSlotRoots)
	{
        if (slotRoot == nullptr)
            continue;

        if (isVisible)
        {
            slotRoot->Open();
        }
        else
        {
            slotRoot->Close();
        }
	}
}

void CharacterProgression_Panel::Show_AvailableSkillSlots(bool isVisible)
{
	for (AvailableSkillSlotUI& slot : _availableSkillSlots)
	{
        if (slot.root == nullptr)
            continue;

		if (isVisible)
        {
            slot.root->Open();
        }
        else
        {
            slot.root->Close();
        }
	}
}

Vec2 CharacterProgression_Panel::Get_AvailableSkillSlotPosition(size_t slotIndex)
{
	constexpr float startX = 125.f;
    constexpr float startY = 112.f;
    constexpr float columnGap = 250.f;
    constexpr float rowGap = 54.f;

    return Vec2(
        startX + static_cast<float>(slotIndex % 2) * columnGap,
        startY + static_cast<float>(slotIndex / 2) * rowGap);
}

wstring CharacterProgression_Panel::Resolve_SkillPortraitTexturePath(HeroId heroId,
    const GameDataTable::SkillTreeDesc* skillTreeDesc)
{
    if (skillTreeDesc == nullptr || skillTreeDesc->portraitTextureName.empty())
        return L"";

    string portraitName = skillTreeDesc->portraitTextureName;
    replace(portraitName.begin(), portraitName.end(), '\\', '/');

    if (fs::path(String::ToWString(portraitName)).extension().empty())
        portraitName += ".png";

    if (portraitName.find('/') != string::npos)
        return ResourceLoader::Resolve_RuntimePath(String::ToWString(portraitName));

    const wstring path =
        L"../Bin/Resources/Textures/UI/Progression/Skills/" +
        String::ToWString(Get_CharacterId(heroId)) +
        L"/" +
        String::ToWString(portraitName);

	return ResourceLoader::Resolve_RuntimePath(path);
}

pair<PreviewAnimState, PreviewAnimState> CharacterProgression_Panel::Resolve_PreviewAnimationForTab(
    ContentTab tab) const
{
    switch (tab)
    {
    case ContentTab::Weapon:
        return { PreviewAnimState::Weapon_Intro, PreviewAnimState::Weapon_Idle };
    case ContentTab::Skill:
        return { PreviewAnimState::Skill_Intro, PreviewAnimState::Skill_Idle };
    case ContentTab::Wardrobe:
        return { PreviewAnimState::Wardrobe_Intro, PreviewAnimState::Wardrobe_Idle };
    case ContentTab::Pictos:
        return { PreviewAnimState::Pictos_Intro, PreviewAnimState::Pictos_Idle };
    case ContentTab::Overview:
    default:
        return { PreviewAnimState::Overview_Intro, PreviewAnimState::Overview_Idle };
    }
}

void CharacterProgression_Panel::Play_SelectedHeroPreviewAnimationForTab(ContentTab tab)
{
    const HeroId heroId = (_selectedHeroId == HeroId::END) ? Get_DefaultSelectedHeroId() : _selectedHeroId;
    const auto [introAnim, idleAnim] = Resolve_PreviewAnimationForTab(tab);

    CLIENT->Play_PreviewAnimation(heroId, introAnim, idleAnim);
}

void CharacterProgression_Panel::Play_SelectedHeroProgressionIdle()
{
    const HeroId heroId = (_selectedHeroId == HeroId::END) ? Get_DefaultSelectedHeroId() : _selectedHeroId;

    CLIENT->Play_PreviewAnimation(
        heroId,
        PreviewAnimState::Overview_Intro,
        PreviewAnimState::Overview_Idle); // 하위 탭 유지 중 캐릭터만 바뀌면 기본 Progression 자세로 되돌린다.
}

HRESULT CharacterProgression_Panel::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Character_Progression_Path;
    desc.prototypeTagPrefix = L"CharacterProgression";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_Character_Progression");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT CharacterProgression_Panel::Bind_UI()
{
    _characterButtons[0] = Find_TypedUI<UI_Button>(_uiByName, L"Button_CharacterSelect_01", "CharacterProgression");
    _characterButtons[1] = Find_TypedUI<UI_Button>(_uiByName, L"Button_CharacterSelect_02", "CharacterProgression");
    _characterButtons[2] = Find_TypedUI<UI_Button>(_uiByName, L"Button_CharacterSelect_03", "CharacterProgression");

    _characterStains[0] = Find_TypedUI<UI_Image>(_uiByName, L"Image_CharacterSelectButton_Stain_01", "CharacterProgression");
    _characterStains[1] = Find_TypedUI<UI_Image>(_uiByName, L"Image_CharacterSelectButton_Stain_02", "CharacterProgression");
    _characterStains[2] = Find_TypedUI<UI_Image>(_uiByName, L"Image_CharacterSelectButton_Stain_03", "CharacterProgression");

    _currentWeaponButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_Weapon", "CharacterProgression");

    _skillButtons[0] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_Skill_1", "CharacterProgression");
    _skillButtons[1] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_Skill_2", "CharacterProgression");
    _skillButtons[2] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_Skill_3", "CharacterProgression");
    _skillButtons[3] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_Skill_4", "CharacterProgression");
    _skillButtons[4] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_Skill_5", "CharacterProgression");
    _skillButtons[5] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_Gradient", "CharacterProgression");

    _skillTreeButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_SkillTree", "CharacterProgression");
    _availableSkillButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_Available_Skill", "CharacterProgression");

    _firstPictosButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_First_Pictos", "CharacterProgression");
    _secondPictosButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_Second_Pictos", "CharacterProgression");
    _thirdPictosButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_Third_Pictos", "CharacterProgression");
    _wardrobeButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_Wardrobe", "CharacterProgression");
    _currentPictosNameTexts[0] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_First_Pictos", "CharacterProgression");
    _currentPictosNameTexts[1] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Second_Pictos", "CharacterProgression");
    _currentPictosNameTexts[2] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Third_Pictos", "CharacterProgression");
    _currentPictosMarkImages[0] = Find_TypedUI<UI_Image>(_uiByName, L"Image_Current_First_Pictos_Mark", "CharacterProgression");
    _currentPictosMarkImages[1] = Find_TypedUI<UI_Image>(_uiByName, L"Image_Current_Second_Pictos_Mark", "CharacterProgression");
    _currentPictosMarkImages[2] = Find_TypedUI<UI_Image>(_uiByName, L"Image_Current_Third_Pictos_Mark", "CharacterProgression");
    _currentPictosEmptyImages[0] = Find_UI(_uiByName, L"Image_Current_First_Pictos_Empty");
    _currentPictosEmptyImages[1] = Find_UI(_uiByName, L"Image_Current_Second_Pictos_Empty");
    _currentPictosEmptyImages[2] = Find_UI(_uiByName, L"Image_Current_Third_Pictos_Empty");

    _characterNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Character_Name", "CharacterProgression");
    _currentLevelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentLevel", "CharacterProgression");
    _currentExpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Character_Current_EXP", "CharacterProgression");
    _maxExpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Character_Max_EXP", "CharacterProgression");
    _currentHpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Character_Current_HP", "CharacterProgression");
    _maxHpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Character_Max_HP", "CharacterProgression");
    _expGaugeImage = Find_TypedUI<UI_Image>(_uiByName, L"Text_Character_EXP_Gauge", "CharacterProgression");
    _hpGaugeImage = Find_TypedUI<UI_Image>(_uiByName, L"Text_Character_HP_Gauge", "CharacterProgression");

    _currentStatHpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatHP", "CharacterProgression");
    _currentStatAttackText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatAttack", "CharacterProgression");
    _currentStatDefenseText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatDefense", "CharacterProgression");
    _currentStatSpeedText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatSpeed", "CharacterProgression");
    _currentStatCriticalPercentText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatCriticalPercent", "CharacterProgression");

    _currentWeaponText.weaponNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Weapon", "CharacterProgression");
    _currentWeaponText.levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Weapon_Level_Value", "CharacterProgression");
    _currentWeaponPanelRoot = Find_UI(_uiByName, L"Panel_Current_Weapon");
    _currentPictosPanelRoot = Find_UI(_uiByName, L"Panel_Current_Pictos");
    _wardrobePanelRoot = Find_UI(_uiByName, L"Panel_Wardrobe");
    _tintPanelRoot = Find_UI(_uiByName, L"Panel_Tint");
    _colimThumbnailPanel = Find_UI(_uiByName, L"Panel_Colim_Thumnail");
    _snowimThumbnailPanel = Find_UI(_uiByName, L"Panel_Snowim_Thumnail");
    _baguetteThumbnailPanel = Find_UI(_uiByName, L"Panel_Baguette_Thumnail");
    _lanceramThumbnailPanel = Find_UI(_uiByName, L"Panel_Lanceram_Thumnail");
    _candyThumbnailPanel = Find_UI(_uiByName, L"Panel_Candy_Thumnail");
    _medalumThumbnailPanel = Find_UI(_uiByName, L"Panel_Medalum_Thumnail");
    _sekarumThumbnailPanel = Find_UI(_uiByName, L"Panel_Sekarum_Thumnail");

    _currentSkillNameTexts[0] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_1", "CharacterProgression");
    _currentSkillNameTexts[1] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_2", "CharacterProgression");
    _currentSkillNameTexts[2] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_3", "CharacterProgression");
    _currentSkillNameTexts[3] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_4", "CharacterProgression");
    _currentSkillNameTexts[4] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_5", "CharacterProgression");

    _currentSkillApTexts[0] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_AP_1", "CharacterProgression");
    _currentSkillApTexts[1] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_AP_2", "CharacterProgression");
    _currentSkillApTexts[2] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_AP_3", "CharacterProgression");
    _currentSkillApTexts[3] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_AP_4", "CharacterProgression");
    _currentSkillApTexts[4] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_AP_5", "CharacterProgression");

    _currentSkillApBgTexts[0] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_AP_BG_1", "CharacterProgression");
    _currentSkillApBgTexts[1] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_AP_BG_2", "CharacterProgression");
    _currentSkillApBgTexts[2] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_AP_BG_3", "CharacterProgression");
    _currentSkillApBgTexts[3] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_AP_BG_4", "CharacterProgression");
    _currentSkillApBgTexts[4] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_AP_BG_5", "CharacterProgression");

    _currentSkillIconImages[0] = Find_TypedUI<UI_Image>(_uiByName, L"Image_Current_Skill_1", "CharacterProgression");
    _currentSkillIconImages[1] = Find_TypedUI<UI_Image>(_uiByName, L"Image_Current_Skill_2", "CharacterProgression");
    _currentSkillIconImages[2] = Find_TypedUI<UI_Image>(_uiByName, L"Image_Current_Skill_3", "CharacterProgression");
    _currentSkillIconImages[3] = Find_TypedUI<UI_Image>(_uiByName, L"Image_Current_Skill_4", "CharacterProgression");
    _currentSkillIconImages[4] = Find_TypedUI<UI_Image>(_uiByName, L"Image_Current_Skill_5", "CharacterProgression");

    _currentSkillEmptyLines[0] = Find_UI(_uiByName, L"Image_Empty_1");
    _currentSkillEmptyLines[1] = Find_UI(_uiByName, L"Image_Empty_2");
    _currentSkillEmptyLines[2] = Find_UI(_uiByName, L"Image_Empty_3");
    _currentSkillEmptyLines[3] = Find_UI(_uiByName, L"Image_Empty_4");
    _currentSkillEmptyLines[4] = Find_UI(_uiByName, L"Image_Empty_5");

    _gradientText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Gradient", "CharacterProgression");
    _gradientGpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Gradient_GP", "CharacterProgression");
    _gradientGpBgText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Gradient_GP_BG", "CharacterProgression");
    _gradientIconImage = Find_TypedUI<UI_Image>(_uiByName, L"Image_Gradient", "CharacterProgression");
    _gradientEmptyLine = Find_UI(_uiByName, L"Image_Empty_Gradient");

    _abilityPanelRoot = Find_UI(_uiByName, L"Panel_Character_Ability");
    _abilityPentagonFrame = Find_TypedUI<UI_Image>(_uiByName, L"Image_Character_Ability_Frame", "CharacterProgression");
    _abilityPentagonAnchor = Find_TypedUI<UI_Image>(_uiByName, L"Image_Ability_Pentagon", "CharacterProgression");

    _currentVitalityText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentVitality", "CharacterProgression");
    _currentMightText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentMight", "CharacterProgression");
    _currentAgilityText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentAgility", "CharacterProgression");
    _currentDefensePrimaryText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentDefense", "CharacterProgression");
    _currentLuckText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentLuck", "CharacterProgression");

	_skillPanelRoot = Find_UI(_uiByName, L"Panel_Skill");
    _skillTitleRoot = Find_UI(_uiByName, L"Text_Skill");

	_equippedSkillSlotRoots[0] = Find_UI(_uiByName, L"Panel_Current_Skill_1");
	_equippedSkillSlotRoots[1] = Find_UI(_uiByName, L"Panel_Current_Skill_2");
	_equippedSkillSlotRoots[2] = Find_UI(_uiByName, L"Panel_Current_Skill_3");
	_equippedSkillSlotRoots[3] = Find_UI(_uiByName, L"Panel_Current_Skill_4");
	_equippedSkillSlotRoots[4] = Find_UI(_uiByName, L"Panel_Current_Skill_5");
	_equippedSkillSlotRoots[5] = Find_UI(_uiByName, L"Panel_Current_Gradient");

    CHECK_NULL(_characterButtons[0], E_FAIL);
    CHECK_NULL(_characterButtons[1], E_FAIL);
    CHECK_NULL(_characterButtons[2], E_FAIL);
    CHECK_NULL(_currentWeaponButton, E_FAIL);
    CHECK_NULL(_firstPictosButton, E_FAIL);
    CHECK_NULL(_secondPictosButton, E_FAIL);
    CHECK_NULL(_thirdPictosButton, E_FAIL);
    CHECK_NULL(_wardrobeButton, E_FAIL);
    CHECK_NULL(_currentPictosNameTexts[0], E_FAIL);
    CHECK_NULL(_currentPictosNameTexts[1], E_FAIL);
    CHECK_NULL(_currentPictosNameTexts[2], E_FAIL);
    CHECK_NULL(_currentPictosMarkImages[0], E_FAIL);
    CHECK_NULL(_currentPictosMarkImages[1], E_FAIL);
    CHECK_NULL(_currentPictosMarkImages[2], E_FAIL);
    CHECK_NULL(_currentPictosEmptyImages[0], E_FAIL);
    CHECK_NULL(_currentPictosEmptyImages[1], E_FAIL);
    CHECK_NULL(_currentPictosEmptyImages[2], E_FAIL);
    CHECK_NULL(_characterNameText, E_FAIL);
    CHECK_NULL(_currentLevelText, E_FAIL);
    CHECK_NULL(_expGaugeImage, E_FAIL);
    CHECK_NULL(_hpGaugeImage, E_FAIL);
    CHECK_NULL(_currentWeaponText.weaponNameText, E_FAIL);
    CHECK_NULL(_currentWeaponPanelRoot, E_FAIL);
    CHECK_NULL(_currentPictosPanelRoot, E_FAIL);
    CHECK_NULL(_wardrobePanelRoot, E_FAIL);
    CHECK_NULL(_tintPanelRoot, E_FAIL);
    CHECK_NULL(_colimThumbnailPanel, E_FAIL);
    CHECK_NULL(_snowimThumbnailPanel, E_FAIL);
    CHECK_NULL(_baguetteThumbnailPanel, E_FAIL);
    CHECK_NULL(_lanceramThumbnailPanel, E_FAIL);
    CHECK_NULL(_candyThumbnailPanel, E_FAIL);
    CHECK_NULL(_medalumThumbnailPanel, E_FAIL);
    CHECK_NULL(_sekarumThumbnailPanel, E_FAIL);

    CHECK_NULL(_abilityPanelRoot, E_FAIL);
    CHECK_NULL(_abilityPentagonFrame, E_FAIL);
    CHECK_NULL(_abilityPentagonAnchor, E_FAIL);
    CHECK_NULL(_currentVitalityText, E_FAIL);
    CHECK_NULL(_currentMightText, E_FAIL);
    CHECK_NULL(_currentAgilityText, E_FAIL);
    CHECK_NULL(_currentDefensePrimaryText, E_FAIL);
    CHECK_NULL(_currentLuckText, E_FAIL);
    CHECK_NULL(_skillPanelRoot, E_FAIL);
    CHECK_NULL(_skillTitleRoot, E_FAIL);

    _expGaugeImage->Set_UseFillRatio(true); // ChangeWeapon과 같은 UV crop 방식으로 경험치 바를 채운다.
    _hpGaugeImage->Set_UseFillRatio(true);  // HP 바도 같은 방식으로 채운다.

    return S_OK;
}

Unique<CharacterProgression_Panel> CharacterProgression_Panel::Create(uint32 levelIndex)
{
    auto instance = make_unique<CharacterProgression_Panel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : CharacterProgression_Panel");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void CharacterProgression_Panel::Free()
{
    if (_root)
        Close_Immediately(false);

    _root.reset();
    _uiByName.clear();

    for (auto& button : _characterButtons)
        button.reset();
    for (auto& stain : _characterStains)
        stain.reset();
    for (auto& button : _skillButtons)
        button.reset();
    for (auto& text : _currentSkillNameTexts)
        text.reset();
    for (auto& text : _currentSkillApTexts)
        text.reset();
    for (auto& text : _currentSkillApBgTexts)
        text.reset();
    for (auto& image : _currentSkillIconImages)
        image.reset();
    for (auto& line : _currentSkillEmptyLines)
        line.reset();

    _currentWeaponButton.reset();
    _currentWeaponPanelRoot.reset();
    _currentPictosPanelRoot.reset();
    _wardrobePanelRoot.reset();
    _tintPanelRoot.reset();
    _skillTreeButton.reset();
    _availableSkillButton.reset();
    _firstPictosButton.reset();
    _secondPictosButton.reset();
    _thirdPictosButton.reset();
    _wardrobeButton.reset();
    for (auto& text : _currentPictosNameTexts)
        text.reset();
    for (auto& image : _currentPictosMarkImages)
        image.reset();
    for (auto& image : _currentPictosEmptyImages)
        image.reset();

    _characterNameText.reset();
    _currentLevelText.reset();
    _currentExpText.reset();
    _maxExpText.reset();
    _currentHpText.reset();
    _maxHpText.reset();

    _currentStatHpText.reset();
    _currentStatAttackText.reset();
    _currentStatDefenseText.reset();
    _currentStatSpeedText.reset();
    _currentStatCriticalPercentText.reset();

    _currentWeaponText = {};
    _colimThumbnailPanel.reset();
    _snowimThumbnailPanel.reset();
    _baguetteThumbnailPanel.reset();
    _lanceramThumbnailPanel.reset();
    _candyThumbnailPanel.reset();
    _medalumThumbnailPanel.reset();
    _sekarumThumbnailPanel.reset();
    _gradientText.reset();
    _gradientGpText.reset();
    _gradientGpBgText.reset();
    _gradientIconImage.reset();
    _gradientEmptyLine.reset();
    _abilityPanelRoot.reset();
    _abilityPentagonFrame.reset();
    _abilityPentagonAnchor.reset();
    _currentVitalityText.reset();
    _currentMightText.reset();
    _currentAgilityText.reset();
    _currentDefensePrimaryText.reset();
    _currentLuckText.reset();
    _abilityPentagonFill.reset();
    _skillPanelRoot.reset();
    _skillTitleRoot.reset();
    for (auto& slotRoot : _equippedSkillSlotRoots)
        slotRoot.reset();
    _availableSkillSlots.clear();
    _isShowingAvailableSkills = false;
    _changeWeaponPanel = nullptr;
    _changeSkillPanel = nullptr;
    _wardrobePanel = nullptr;
    _picktosPanel = nullptr;
    _pendingPictosSlotIndex = 0;
    _activeContentTab = ContentTab::Overview;

    UI_RuntimePanelBase::Free();
}
