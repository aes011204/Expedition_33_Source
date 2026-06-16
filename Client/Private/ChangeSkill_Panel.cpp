#include "ChangeSkill_Panel.h"

#include "Battle_Enum.h"
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
#include "SkillTreeCom.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"
#include "UIPreviewManager.h"

#include <algorithm>
#include <filesystem>

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

    vector<UIPreviewManager::PreviewSlotDesc> Build_ChangeSkillPreviewLayout()
    {
        return
        {
            { HeroId::Lune,    Vec3(1.1f,  0.f, -0.1f), Vec3(0.f, 0.f, -0.1f) },
            { HeroId::Gustave, Vec3(0.0f,  0.f,  0.f), Vec3(0.f, 0.f,  0.f) },
            { HeroId::Maelle,  Vec3(-1.1f, 0.f, -0.1f), Vec3(0.f, 0.f, -0.1f) },
        };
    }

    void Show_ChangeSkillPreview(HeroId heroId)
    {
        const auto previewLayout = Build_ChangeSkillPreviewLayout();
        CLIENT->Apply_PreviewLayout(previewLayout);
        CLIENT->Show_Preview(heroId, PreviewShotPanel::SkillTree);
    }
}

HRESULT ChangeSkill_Panel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately(false);

    return S_OK;
}

void ChangeSkill_Panel::Update(float timeDelta)
{
    if (!Is_Open())
        return;

    Handle_SelectionInput(); // Q/E 입력으로 현재 보는 영웅을 바꾼다.
    Handle_ButtonInput();    // 내부 버튼 클릭은 아직 로그만 남긴다.
}

void ChangeSkill_Panel::Open()
{
    if (_root == nullptr)
        return;

    if (Is_Open())
        return;

    UI_RuntimePanelBase::Open();
    Set_UIMapOpenState(_uiByName, true);

    if (_selectedHeroId == HeroId::END)
        _selectedHeroId = Get_DefaultSelectedHeroId();

    CLIENT->Begin_Preview(PreviewShotPanel::SkillTree); // Character Progression 하위 메뉴도 같은 preview shot을 이어서 쓴다.
    Show_ChangeSkillPreview(_selectedHeroId);
    Update_PreviewHeroVisibility();
    Apply_PreviewOverrides();
    Refresh_All();

    GAME->Play_SoundCue(Constants::SoundCue::UI_Global_GenericEnter);
    GAME->Unlock_Mouse();
    GAME->Set_GameplayPaused(true); // preview 패널 동안 월드 진행을 멈춘다.
}

void ChangeSkill_Panel::Close()
{
    Close_Immediately(true);
}

void ChangeSkill_Panel::Close_Immediately(bool restoreWorldControl)
{
    Show_AvailableSkillSlots(false);
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

void ChangeSkill_Panel::Close_ForMenuTransition()
{
    Close_Immediately(false);
}

void ChangeSkill_Panel::Set_SelectedHeroId(HeroId heroId)
{
    _selectedHeroId = (heroId == HeroId::END) ? Get_DefaultSelectedHeroId() : heroId;

    if (Is_Open())
    {
        Show_ChangeSkillPreview(_selectedHeroId);
        Update_PreviewHeroVisibility();
        Apply_PreviewOverrides();
        Refresh_All();
    }
}

HRESULT ChangeSkill_Panel::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_ChangeSkill_Path;
    desc.prototypeTagPrefix = L"ChangeSkill";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_ChangeSkill");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT ChangeSkill_Panel::Bind_UI()
{
    _characterButtons[0] = Find_TypedUI<UI_Button>(_uiByName, L"Button_CharacterSelect_01", "ChangeSkill");
    _characterButtons[1] = Find_TypedUI<UI_Button>(_uiByName, L"Button_CharacterSelect_02", "ChangeSkill");
    _characterButtons[2] = Find_TypedUI<UI_Button>(_uiByName, L"Button_CharacterSelect_03", "ChangeSkill");

    _characterStains[0] = Find_TypedUI<UI_Image>(_uiByName, L"Image_CharacterSelectButton_Stain_01", "ChangeSkill");
    _characterStains[1] = Find_TypedUI<UI_Image>(_uiByName, L"Image_CharacterSelectButton_Stain_02", "ChangeSkill");
    _characterStains[2] = Find_TypedUI<UI_Image>(_uiByName, L"Image_CharacterSelectButton_Stain_03", "ChangeSkill");

    _currentSkillButtons[0] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_Skill_1", "ChangeSkill");
    _currentSkillButtons[1] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_Skill_2", "ChangeSkill");
    _currentSkillButtons[2] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_Skill_3", "ChangeSkill");
    _currentSkillButtons[3] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_Skill_4", "ChangeSkill");
    _currentSkillButtons[4] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_Skill_5", "ChangeSkill");

    _gradientButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_Current_Gradient", "ChangeSkill");
    _availableSkillButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_Available_Skill", "ChangeSkill");
    _skillTreeButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_SkillTree", "ChangeSkill");

    _characterNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Character_Name", "ChangeSkill");
    _currentLevelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentLevel", "ChangeSkill");
    _currentExpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Character_Current_EXP", "ChangeSkill");
    _maxExpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Character_Max_EXP", "ChangeSkill");
    _currentHpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Character_Current_HP", "ChangeSkill");
    _maxHpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Character_Max_HP", "ChangeSkill");

    _currentStatHpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatHP", "ChangeSkill");
    _currentStatAttackText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatAttack", "ChangeSkill");
    _currentStatDefenseText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatDefense", "ChangeSkill");
    _currentStatSpeedText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatSpeed", "ChangeSkill");
    _currentStatCriticalPercentText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatCriticalPercent", "ChangeSkill");

    _currentSkillNameTexts[0] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_1", "ChangeSkill");
    _currentSkillNameTexts[1] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_2", "ChangeSkill");
    _currentSkillNameTexts[2] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_3", "ChangeSkill");
    _currentSkillNameTexts[3] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_4", "ChangeSkill");
    _currentSkillNameTexts[4] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_5", "ChangeSkill");

    _currentSkillApTexts[0] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_AP_1", "ChangeSkill");
    _currentSkillApTexts[1] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_AP_2", "ChangeSkill");
    _currentSkillApTexts[2] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_AP_3", "ChangeSkill");
    _currentSkillApTexts[3] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_AP_4", "ChangeSkill");
    _currentSkillApTexts[4] = Find_TypedUI<UI_Text>(_uiByName, L"Text_Current_Skill_AP_5", "ChangeSkill");

    _gradientText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Gradient", "ChangeSkill");
    _gradientGpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Gradient_GP", "ChangeSkill");
    _availableSkillListRoot = Find_UI(_uiByName, L"Panel_AvailableSkill");

    AvailableSkillSlotUI firstAvailableSlot{};
    firstAvailableSlot.root = Find_UI(_uiByName, L"Panel_Available_Skill");
    firstAvailableSlot.button = _availableSkillButton;
    firstAvailableSlot.portraitImage = Find_TypedUI<UI_Image>(_uiByName, L"Image_Available_Skill", "ChangeSkill");
    firstAvailableSlot.nameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Available_Skill", "ChangeSkill");
    firstAvailableSlot.apText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Available_Skill_AP", "ChangeSkill");
    firstAvailableSlot.apBgText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Available_Skill_AP_BG", "ChangeSkill");
    firstAvailableSlot.isOwnedByMainDocument = true;
    _availableSkillSlots.push_back(firstAvailableSlot);

    CHECK_NULL(_characterButtons[0], E_FAIL);
    CHECK_NULL(_characterButtons[1], E_FAIL);
    CHECK_NULL(_characterButtons[2], E_FAIL);
    CHECK_NULL(_characterNameText, E_FAIL);
    CHECK_NULL(_currentLevelText, E_FAIL);
    CHECK_NULL(_availableSkillListRoot, E_FAIL);
    CHECK_NULL(_availableSkillSlots[0].root, E_FAIL);
    CHECK_NULL(_availableSkillSlots[0].button, E_FAIL);
    CHECK_NULL(_availableSkillSlots[0].portraitImage, E_FAIL);
    CHECK_NULL(_availableSkillSlots[0].nameText, E_FAIL);
    CHECK_NULL(_availableSkillSlots[0].apText, E_FAIL);
    CHECK_NULL(_availableSkillSlots[0].apBgText, E_FAIL);

    return S_OK;
}

void ChangeSkill_Panel::Handle_SelectionInput()
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
        Show_ChangeSkillPreview(_selectedHeroId); // Skill 변경 메뉴에서도 Q/E 선택과 preview target을 같이 맞춘다.
        Update_PreviewHeroVisibility();
        Apply_PreviewOverrides();
        Refresh_All();
    }
}

void ChangeSkill_Panel::Handle_ButtonInput()
{
    for (size_t i = 0; i < _currentSkillButtons.size(); ++i)
    {
        if (_currentSkillButtons[i] == nullptr || !_currentSkillButtons[i]->Was_Clicked())
            continue;

        GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);
        LOG_DEBUG("ChangeSkill_Panel: current skill slot clicked. index={}", i);
        return;
    }

    if (_gradientButton && _gradientButton->Was_Clicked())
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);
        LOG_DEBUG("ChangeSkill_Panel: gradient slot clicked");
        return;
    }

    if (_availableSkillButton && _availableSkillButton->Was_Clicked())
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);
        Refresh_AvailableSkills();
        return;
    }

    if (_skillTreeButton && _skillTreeButton->Was_Clicked())
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);
        LOG_DEBUG("ChangeSkill_Panel: skill tree clicked");
    }
}

void ChangeSkill_Panel::Refresh_All()
{
    const Shared<PlayerStatCom> statCom = Resolve_SelectedPlayerStat();

    Refresh_CharacterSelectUI();

    if (statCom == nullptr)
    {
        Show_AvailableSkillSlots(false);
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

        for (size_t i = 0; i < _currentSkillNameTexts.size(); ++i)
        {
            Set_Text(_currentSkillNameTexts[i], L"-");
            Set_Text(_currentSkillApTexts[i], L"0");
        }

        Set_Text(_gradientText, L"-");
        Set_Text(_gradientGpText, L"0");
        return;
    }

    Refresh_CharacterSheet(statCom);
    Refresh_CombatStats(statCom);
    Refresh_CurrentSkills(statCom);
    Refresh_AvailableSkills();
}

void ChangeSkill_Panel::Refresh_CharacterSheet(const Shared<PlayerStatCom>& statCom)
{
    if (statCom == nullptr)
        return;

    const PlayerStatCom::PRuntimeCombatStat& statInfo = statCom->Get_PlayerRuntimeStat();
    const uint32 maxExp = statInfo.CurMaxExp > 0 ? statInfo.CurMaxExp : max(1u, statInfo.nextExp);

    Set_Text(_characterNameText, Get_DisplayHeroName(_selectedHeroId));
    Set_UIntText(_currentLevelText, statInfo.CurLevel);
    Set_UIntText(_currentExpText, statInfo.CurExp);
    Set_UIntText(_maxExpText, maxExp);
    Set_IntText(_currentHpText, static_cast<int32>(statInfo.CurHp));
    Set_IntText(_maxHpText, static_cast<int32>(statInfo.CurMaxHp));
}

void ChangeSkill_Panel::Refresh_CombatStats(const Shared<PlayerStatCom>& statCom)
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

void ChangeSkill_Panel::Refresh_CurrentSkills(const Shared<PlayerStatCom>& statCom)
{
    for (size_t i = 0; i < _currentSkillNameTexts.size(); ++i)
    {
        Set_Text(_currentSkillNameTexts[i], L"-");
        Set_Text(_currentSkillApTexts[i], L"0");
    }

    Set_Text(_gradientText, L"-");
    Set_Text(_gradientGpText, L"0");

    if (statCom == nullptr)
        return;

    const vector<string>& skillKeys = statCom->Get_SkillKeys();
    const size_t normalSkillCount = min<size_t>(_currentSkillNameTexts.size(), skillKeys.size());

    for (size_t i = 0; i < normalSkillCount; ++i)
    {
        const string& skillKey = skillKeys[i];
        const GameDataTable::SkillTreeDesc* skillTreeDesc = CLIENT->Get_SkillTreeDesc(skillKey);
        const Skill_Data* skillData = CLIENT->Get_Skill(skillKey);

        Set_Text(_currentSkillNameTexts[i], skillTreeDesc ? skillTreeDesc->skillName : String::ToWString(skillKey));
        Set_IntText(_currentSkillApTexts[i], skillData ? skillData->apCost : 0);
    }

    if (skillKeys.size() > _currentSkillNameTexts.size())
    {
        const string& gradientSkillKey = skillKeys[_currentSkillNameTexts.size()];
        const GameDataTable::SkillTreeDesc* skillTreeDesc = CLIENT->Get_SkillTreeDesc(gradientSkillKey);
        const Skill_Data* skillData = CLIENT->Get_Skill(gradientSkillKey);

        Set_Text(_gradientText, skillTreeDesc ? skillTreeDesc->skillName : String::ToWString(gradientSkillKey));
        Set_IntText(_gradientGpText, skillData ? skillData->apCost : 0);
    }
}

void ChangeSkill_Panel::Refresh_CharacterSelectUI()
{
    for (size_t i = 0; i < kHeroOrder.size(); ++i)
    {
        if (_characterStains[i] == nullptr)
            continue;

        if (kHeroOrder[i] == _selectedHeroId)
            _characterStains[i]->Open();
        else
            _characterStains[i]->Close();
    }
}

Shared<PlayerStatCom> ChangeSkill_Panel::Resolve_SelectedPlayerStat() const
{
    const HeroId heroId = (_selectedHeroId == HeroId::END) ? Get_DefaultSelectedHeroId() : _selectedHeroId;

    const Shared<Player_WorldBase> hero = CLIENT->Find_Hero(heroId);
    if (hero == nullptr)
        return nullptr;

    return hero->Get_Component<PlayerStatCom>();
}

Shared<SkillTreeCom> ChangeSkill_Panel::Resolve_SelectedSkillTree() const
{
    const HeroId heroId = (_selectedHeroId == HeroId::END) ? Get_DefaultSelectedHeroId() : _selectedHeroId;

    const Shared<Player_WorldBase> hero = CLIENT->Find_Hero(heroId);
    if (hero == nullptr)
        return nullptr;

    return hero->Get_Component<SkillTreeCom>();
}

HeroId ChangeSkill_Panel::Get_DefaultSelectedHeroId()
{
    const vector<HeroPartyEntry>& partyEntries = CLIENT->Get_HeroEntries();
    if (!partyEntries.empty())
        return partyEntries.front().heroId;

    return HeroId::Gustave;
}

string ChangeSkill_Panel::Get_CharacterId(HeroId heroId)
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

wstring ChangeSkill_Panel::Get_DisplayHeroName(HeroId heroId)
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

void ChangeSkill_Panel::Set_Text(const Shared<Engine::UI_Text>& text, const wstring& value)
{
    if (text)
        text->Set_Text(value);
}

void ChangeSkill_Panel::Refresh_AvailableSkills()
{
    const Shared<SkillTreeCom> skillTreeCom = Resolve_SelectedSkillTree();
    if (skillTreeCom == nullptr)
    {
        Show_AvailableSkillSlots(false);
        return;
    }

    const vector<string>& unlockedSkillIds = skillTreeCom->Get_UnlockedSkillIds();
    if (FAILED(Ensure_AvailableSkillSlotCount(unlockedSkillIds.size())))
        return;

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

HRESULT ChangeSkill_Panel::Ensure_AvailableSkillSlotCount(size_t slotCount)
{
    CHECK_NULL(_availableSkillListRoot, E_FAIL);

    while (_availableSkillSlots.size() < slotCount)
    {
        const size_t slotIndex = _availableSkillSlots.size();

        AvailableSkillSlotUI slot{};

        UI_RuntimeLoader::UI_LoadDesc desc{};
        desc.levelIndex = _levelIndex;
        desc.jsonPath = Constants::UI::UI_Available_Skill_Path;
        desc.prototypeTagPrefix = L"ChangeSkillAvailableSkill_" + to_wstring(slotIndex);
        desc.outUIByName = &slot.uiByName;

        CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

        slot.root = Find_UI(slot.uiByName, L"Panel_Available_Skill");
        slot.button = Find_TypedUI<UI_Button>(slot.uiByName, L"Button_Available_Skill", "ChangeSkillAvailableSkill");
        slot.portraitImage = Find_TypedUI<UI_Image>(slot.uiByName, L"Image_Available_Skill", "ChangeSkillAvailableSkill");
        slot.nameText = Find_TypedUI<UI_Text>(slot.uiByName, L"Text_Available_Skill", "ChangeSkillAvailableSkill");
        slot.apText = Find_TypedUI<UI_Text>(slot.uiByName, L"Text_Available_Skill_AP", "ChangeSkillAvailableSkill");
        slot.apBgText = Find_TypedUI<UI_Text>(slot.uiByName, L"Text_Available_Skill_AP_BG", "ChangeSkillAvailableSkill");

        CHECK_NULL(slot.root, E_FAIL);
        CHECK_NULL(slot.button, E_FAIL);
        CHECK_NULL(slot.portraitImage, E_FAIL);
        CHECK_NULL(slot.nameText, E_FAIL);
        CHECK_NULL(slot.apText, E_FAIL);
        CHECK_NULL(slot.apBgText, E_FAIL);

        slot.root->Set_Parent(_availableSkillListRoot); // ChangeSkill의 Available Skill 리스트 안에 추가 카드만 붙인다.
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

void ChangeSkill_Panel::Apply_AvailableSkillSlot(size_t slotIndex, const string& skillId)
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
    {
        slot.portraitImage->Set_TexturePath(portraitPath);
        slot.portraitImage->Open();
    }
    else if (slot.portraitImage)
    {
        slot.portraitImage->Close();
    }
}

void ChangeSkill_Panel::Show_AvailableSkillSlots(bool isVisible)
{
    for (AvailableSkillSlotUI& slot : _availableSkillSlots)
    {
        if (slot.root == nullptr)
            continue;

        if (isVisible)
            slot.root->Open();
        else
            slot.root->Close();
    }
}

Vec2 ChangeSkill_Panel::Get_AvailableSkillSlotPosition(size_t slotIndex)
{
    constexpr float startX = 206.f;
    constexpr float startY = 100.f;
    constexpr float columnGap = 250.f;
    constexpr float rowGap = 54.f;

    return Vec2(
        startX + static_cast<float>(slotIndex % 2) * columnGap,
        startY + static_cast<float>(slotIndex / 2) * rowGap);
}

wstring ChangeSkill_Panel::Resolve_SkillPortraitTexturePath(HeroId heroId,
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

void ChangeSkill_Panel::Apply_PreviewOverrides()
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

void ChangeSkill_Panel::Restore_PreviewOverrides()
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

void ChangeSkill_Panel::Apply_PreviewRimState()
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

void ChangeSkill_Panel::Clear_PreviewRimState()
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

void ChangeSkill_Panel::Update_PreviewHeroVisibility()
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

Unique<ChangeSkill_Panel> ChangeSkill_Panel::Create(uint32 levelIndex)
{
    auto instance = make_unique<ChangeSkill_Panel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : ChangeSkill_Panel");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void ChangeSkill_Panel::Free()
{
    if (_root)
        Close_Immediately(false);

    _root.reset();
    _uiByName.clear();

    for (auto& button : _characterButtons)
        button.reset();
    for (auto& stain : _characterStains)
        stain.reset();
    for (auto& button : _currentSkillButtons)
        button.reset();
    for (auto& text : _currentSkillNameTexts)
        text.reset();
    for (auto& text : _currentSkillApTexts)
        text.reset();

    _gradientButton.reset();
    _availableSkillButton.reset();
    _skillTreeButton.reset();

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
    _gradientText.reset();
    _gradientGpText.reset();
    _availableSkillListRoot.reset();
    _availableSkillSlots.clear();

    UI_RuntimePanelBase::Free();
}
