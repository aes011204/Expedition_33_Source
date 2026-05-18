#include "CharacterStatus_Panel.h"
#include "ClientInstance.h"
#include "Client_Constants.h"
#include "CombatStatCom.h"
#include "GameInstance.h"
#include "Player_WorldBase.h"
#include "PlayerStatCom.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UIPreviewManager.h"

namespace
{
    constexpr HeroId characterStatusOrder[] =
    {
        HeroId::Lune,
        HeroId::Gustave,
        HeroId::Maelle,
    };

    HeroId Get_SelectedCharacterStatusHeroId(uint32 selectedHeroIndex)
    {
        if (selectedHeroIndex >= _countof(characterStatusOrder))
            return HeroId::Gustave;

        return characterStatusOrder[selectedHeroIndex];
    }

    uint32 Get_CharacterStatusIndex(HeroId heroId)
    {
        for (uint32 i = 0; i < _countof(characterStatusOrder); ++i)
        {
            if (characterStatusOrder[i] == heroId)
                return i;
        }

        return 0;
    }
}

HRESULT CharacterStatus_Panel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();
    
    return S_OK;
}

void CharacterStatus_Panel::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!_isOpen)
        return;

    Handle_SelectionInput();
}

void CharacterStatus_Panel::Open()
{
    _isOpen = true;

    if (_root)
        _root->Open();

    const HeroId activeHeroId = CLIENT->Get_HeroId(CLIENT->Get_ActiveHeroSlot());
    _selectedHeroIndex = Get_CharacterStatusIndex(activeHeroId);

    CLIENT->Begin_Preview(PreviewShotTable::PreviewShotPanel::CharacterStatus);

    auto previewLayout = Build_PreviewLayout();
    CLIENT->Apply_PreviewLayout(previewLayout);
    CLIENT->Show_Preview(Get_SelectedCharacterStatusHeroId(_selectedHeroIndex), PreviewShotTable::PreviewShotPanel::CharacterStatus);

    Refresh_CombatStats();
    Refresh_CharacterSelectUI();

    GAME->Unlock_Mouse();
    GAME->Set_GameplayPaused(true);
}

void CharacterStatus_Panel::Close()
{
    _isOpen = false;

    if (_root)
        _root->Close();

    CLIENT->Hide_Preview();

    GAME->Set_GameplayPaused(false);
    GAME->Lock_Mouse();
}

void CharacterStatus_Panel::Toggle()
{
    if (_isOpen)
        Close();
    else
        Open();
}

void CharacterStatus_Panel::Handle_SelectionInput()
{
    const vector<HeroPartyEntry>& partyEntries = CLIENT->Get_HeroEntries();
    if (partyEntries.empty())
        return;

    const uint32 heroCount = static_cast<uint32>(partyEntries.size());
    uint32 previousIndex = _selectedHeroIndex;

    if (GAME->KeyDown(KEY_TYPE::Q))
    {
        _selectedHeroIndex = (_selectedHeroIndex + heroCount - 1) % heroCount;
    }
    if (GAME->KeyDown(KEY_TYPE::E))
    {
        _selectedHeroIndex = (_selectedHeroIndex + 1) % heroCount;
    }

    if (previousIndex != _selectedHeroIndex)
    {
        Refresh_CombatStats();
        Refresh_CharacterSelectUI();

        auto previewLayout = Build_PreviewLayout();
        CLIENT->Apply_PreviewLayout(previewLayout);
        CLIENT->Show_Preview(Get_SelectedCharacterStatusHeroId(_selectedHeroIndex), PreviewShotTable::PreviewShotPanel::CharacterStatus);
    }
}

uint32 CharacterStatus_Panel::Get_WrappedPartyIndex(int32 index) const
{
    const vector<HeroPartyEntry>& partyEntries = CLIENT->Get_HeroEntries();
    const int32 heroCount = static_cast<int32>(partyEntries.size());

    if (heroCount <= 0)
        return 0;

    int32 wrappedIndex = index % heroCount;
    if (wrappedIndex < 0)
        wrappedIndex += heroCount;

    return static_cast<uint32>(wrappedIndex);
}

HRESULT CharacterStatus_Panel::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_CharacterStatus_Path;
    desc.prototypeTagPrefix = L"CharacterStatus";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_CharacterStatus");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT CharacterStatus_Panel::Bind_UI()
{
    _levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentLevel");
    _statPointText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatPoint");

    _vitalityText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentVitality");
    _mightText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentMight");
    _agilityText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentAgility");
    _defenseStatText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentDefense");
    _luckText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentLuck");

    _hpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatHP");
    _attackText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatAttack");
    _speedText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatSpeed");
    _defenseValueText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatDefense");
    _criticalRateText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatCriticalPercent");

    _selectButton01 = Find_UI(_uiByName, L"Button_CharacterSelect_01");
    _selectButton02 = Find_UI(_uiByName, L"Button_CharacterSelect_02");
    _selectButton03 = Find_UI(_uiByName, L"Button_CharacterSelect_03");

    _selectFrame01 = Find_UI(_uiByName, L"Image_CharacterSelectButton_01");
    _selectFrame02 = Find_UI(_uiByName, L"Image_CharacterSelectButton_02");
    _selectFrame03 = Find_UI(_uiByName, L"Image_CharacterSelectButton_03");

    _selectPortrait01 = Find_UI(_uiByName, L"Image_Lune");
    _selectPortrait02 = Find_UI(_uiByName, L"Image_Gustave");
    _selectPortrait03 = Find_UI(_uiByName, L"Image_Maelle");

    _selectIndicator01 = Find_UI(_uiByName, L"Image_PartyIndicator_01");
    _selectIndicator02 = Find_UI(_uiByName, L"Image_PartyIndicator_02");
    _selectIndicator03 = Find_UI(_uiByName, L"Image_PartyIndicator_03");

    return S_OK;
}

void CharacterStatus_Panel::Refresh_CombatStats()
{
    Shared<Player_WorldBase> player = CLIENT->Find_Hero(Get_SelectedCharacterStatusHeroId(_selectedHeroIndex));
    CHECK_NULL(player);

    Shared<PlayerStatCom> playerStat = player->Get_Component<PlayerStatCom>();
    CHECK_NULL(playerStat);

    const PlayerStatCom::PRuntimeCombatStat& statInfo = playerStat->Get_PlayerRuntimeStat();

    // 현재 선택된 가운데 hero 기준으로 표시
    Set_UIntText(_levelText, statInfo.CurLevel);
    Set_UIntText(_statPointText, statInfo.CurStatPoint);

    Set_IntText(_vitalityText, statInfo.CurVitality);
    Set_IntText(_mightText, statInfo.CurMight);
    Set_IntText(_agilityText, statInfo.CurAgility);
    Set_IntText(_defenseStatText, statInfo.CurDefense);
    Set_IntText(_luckText, statInfo.CurLuck);
    
    Set_HpText(_hpText, statInfo.CurHp, statInfo.CurMaxHp);
    Set_IntText(_attackText, static_cast<int32>(statInfo.CurPhysicalAttack));
    Set_IntText(_speedText, static_cast<int32>(statInfo.CurSpeed));
    Set_IntText(_defenseValueText, static_cast<int32>(statInfo.CurPhysicalDefense));
    Set_PercentText(_criticalRateText, statInfo.CurCriticalRate);

}

void CharacterStatus_Panel::Refresh_CharacterSelectUI()
{
    static const Vec2 kSelectedScale = Vec2(1.4f, 1.4f);
    static const Vec2 kNormalScale = Vec2(1.f, 1.f);

    Shared<UI_Base> buttons[3] =
    {
        _selectButton01, _selectButton02, _selectButton03,
    };

    Shared<UI_Base> frames[3] =
    {
        _selectFrame01, _selectFrame02, _selectFrame03,
    };

    Shared<UI_Base> portraits[3] =
    {
        _selectPortrait01, _selectPortrait02, _selectPortrait03,
    };

    Shared<UI_Base> indicators[3] =
    {
        _selectIndicator01, _selectIndicator02, _selectIndicator03,
    };

    for (uint32 i = 0; i < 3; i++)
    {
        const bool isSelected = (i == _selectedHeroIndex);
        const Vec2 targetScale = isSelected ? kSelectedScale : kNormalScale;

        if (buttons[i] && buttons[i]->Get_Transform())
            buttons[i]->Get_Transform()->Set_Scale(targetScale);

        if (frames[i] && frames[i]->Get_Transform())
            frames[i]->Get_Transform()->Set_Scale(targetScale);

        if (portraits[i] && portraits[i]->Get_Transform())
            portraits[i]->Get_Transform()->Set_Scale(targetScale);

        if (indicators[i] && indicators[i]->Get_Transform())
            indicators[i]->Get_Transform()->Set_Scale(targetScale);
    }
}

Shared<UI_Base> CharacterStatus_Panel::Find_UI(const umap<wstring, Shared<UI_Base>>& uiByName, const wstring& name) const
{
    const auto iter = uiByName.find(name);
    if (iter == uiByName.end() || iter->second == nullptr)
        return nullptr;

    return iter->second;
}

void CharacterStatus_Panel::Set_IntText(const Shared<UI_Text>& text, int32 value)
{
    if (text)
        text->Set_Text(to_wstring(value));
}

void CharacterStatus_Panel::Set_UIntText(const Shared<UI_Text>& text, uint32 value)
{
    if (text)
        text->Set_Text(to_wstring(value));
}

void CharacterStatus_Panel::Set_PercentText(const Shared<UI_Text>& text, float value)
{
    if (text)
        text->Set_Text(to_wstring(static_cast<int32>(value)) + L"%");
}

void CharacterStatus_Panel::Set_HpText(const Shared<UI_Text>& text, float hp, float maxHp)
{
    if (text == nullptr)
        return;

    const int32 hpValue = static_cast<int32>(hp);
    const int32 maxHpValue = static_cast<int32>(maxHp);

    text->Set_Text(to_wstring(hpValue) + L" / " + to_wstring(maxHpValue));
}

vector<UIPreviewManager::PreviewSlotDesc> CharacterStatus_Panel::Build_PreviewLayout()
{
    static const Vec3 positions[] =
    {
        Vec3(-1.1f, 0.f, -0.1f),
        Vec3(0.0f,  0.f, 0.f),
        Vec3(1.1f,  0.f, -0.1f),
    };

    static const Vec3 rotations[] =
    {
        Vec3(0.f, 0.f, -0.1f),
        Vec3(0.f, 0.f, 0.f),
        Vec3(0.f, 0.f, -0.1f),
    };

    vector<UIPreviewManager::PreviewSlotDesc> layout;
    layout.reserve(_countof(characterStatusOrder));

    for (uint32 i = 0; i < _countof(characterStatusOrder); ++i)
    {
        layout.push_back({ characterStatusOrder[i], positions[i], rotations[i] });
    }

    return layout;
}

Unique<CharacterStatus_Panel> CharacterStatus_Panel::Create(uint32 levelIndex)
{
    auto instance = make_unique<CharacterStatus_Panel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : CharacterStatus_Panel");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void CharacterStatus_Panel::Free()
{
    if (_isOpen)
        CLIENT->Hide_Preview();

    if (_root)
        _root->Close();

    _root.reset();

    _levelText.reset();
    _statPointText.reset();

    _vitalityText.reset();
    _mightText.reset();
    _agilityText.reset();
    _defenseStatText.reset();
    _luckText.reset();

    _hpText.reset();
    _attackText.reset();
    _speedText.reset();
    _defenseValueText.reset();
    _criticalRateText.reset();

    _uiByName.clear();
    _isOpen = false;

    GAME->Set_GameplayPaused(false);

    Base::Free();
}

// --- GamePlay에 있던 UI 버튼 클릭 처리
//
//namespace
//{
//constexpr int32 TEST_HP_STEP = 50;
//constexpr int32 TEST_EXP_STEP = 25;
//
//wstring Make_TestStatText(int32 value, int32 maxValue)
//{
//    return to_wstring(value) + L"    /    " + to_wstring(maxValue);
//}
//
//Shared<UI_Base> Find_NamedBaseUI(const unordered_map<wstring, Shared<UI_Base>>& uiByName, const wstring& name)
//{
//    const auto iter = uiByName.find(name);
//    if (iter == uiByName.end() || nullptr == iter->second)
//    {
//        LOG_WARN("Test Missing Object: {}", String::ToString(name));
//        return nullptr;
//    }
//
//    return iter->second;
//}
//
//template <typename T>
//Shared<T> Find_NamedUI(const unordered_map<wstring, Shared<UI_Base>>& uiByName, const wstring& name)
//{
//    const Shared<UI_Base> ui = Find_NamedBaseUI(uiByName, name);
//    if (nullptr == ui)
//        return nullptr;
//
//    Shared<T> typedUI = dynamic_pointer_cast<T>(ui);
//    if (nullptr == typedUI)
//        LOG_WARN("Test Type Mismatch: {}", String::ToString(name));
//
//    return typedUI;
//}
//}
//
//HRESULT Level_GamePlay::Bind_TestUI()
//{
//    _hpText = Find_NamedUI<UI_Text>(_uiByName, L"Lune_HP_Text");
//    _expText = Find_NamedUI<UI_Text>(_uiByName, L"Lune_EXP_Text");
//    _hpGauge = Find_NamedBaseUI(_uiByName, L"Lune_HP_Gauge");
//    _expGauge = Find_NamedBaseUI(_uiByName, L"Lune_EXP_Gauge");
//    _addHpButton = Find_NamedUI<UI_Button>(_uiByName, L"Add_HP");
//    _removeHpButton = Find_NamedUI<UI_Button>(_uiByName, L"Remove_HP");
//    _addExpButton = Find_NamedUI<UI_Button>(_uiByName, L"Add_EXP");
//    _removeExpButton = Find_NamedUI<UI_Button>(_uiByName, L"Remove_EXP");
//
//    if (nullptr != _hpGauge && nullptr != _hpGauge->Get_Transform())
//        _hpGaugeOriginalSize = _hpGauge->Get_Transform()->Get_Size();
//    else
//        LOG_WARN("Failed HP Gauge");
//
//    if (nullptr != _expGauge && nullptr != _expGauge->Get_Transform())
//        _expGaugeOriginalSize = _expGauge->Get_Transform()->Get_Size();
//    else
//        LOG_WARN("Failed EXP Gauge");
//
//    Update_TestUI();
//
//    return S_OK;
//}
//
//void Level_GamePlay::Update_Input_TestUI()
//{
//    bool needsRefresh = false;
//
//    if (nullptr != _addHpButton && _addHpButton->Was_Clicked())
//    {
//        _testCurrentHp = std::clamp(_testCurrentHp + TEST_HP_STEP, 0, _testMaxHp);
//        needsRefresh = true;
//    }
//
//    if (nullptr != _removeHpButton && _removeHpButton->Was_Clicked())
//    {
//        _testCurrentHp = std::clamp(_testCurrentHp - TEST_HP_STEP, 0, _testMaxHp);
//        needsRefresh = true;
//    }
//
//    if (nullptr != _addExpButton && _addExpButton->Was_Clicked())
//    {
//        _testCurrentExp = std::clamp(_testCurrentExp + TEST_EXP_STEP, 0, _testMaxExp);
//        needsRefresh = true;
//    }
//
//    if (nullptr != _removeExpButton && _removeExpButton->Was_Clicked())
//    {
//        _testCurrentExp = std::clamp(_testCurrentExp - TEST_EXP_STEP, 0, _testMaxExp);
//        needsRefresh = true;
//    }
//
//    if (needsRefresh)
//        Update_TestUI();
//}
//
//void Level_GamePlay::Update_TestUI()
//{
//    if (nullptr != _hpText)
//        _hpText->Set_Text(Make_TestStatText(_testCurrentHp, _testMaxHp));
//
//    if (nullptr != _expText)
//        _expText->Set_Text(Make_TestStatText(_testCurrentExp, _testMaxExp));
//
//    ReSize_TestGauge(_hpGauge, _hpGaugeOriginalSize, _testCurrentHp, _testMaxHp);
//    ReSize_TestGauge(_expGauge, _expGaugeOriginalSize, _testCurrentExp, _testMaxExp);
//}
