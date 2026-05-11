#include "CharacterStatus_Panel.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"
#include "GameInstance.h"
#include "Player.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"

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
    if (!_isOpen)
        return;

    Refresh_CombatStats();
}

void CharacterStatus_Panel::Open()
{
    _isOpen = true;

    if (_root)
        _root->Open();

    Refresh_CombatStats();

    GAME->Unlock_Mouse();
    GAME->Set_GameplayPaused(true);
}

void CharacterStatus_Panel::Close()
{
    _isOpen = false;

    if (_root)
        _root->Close();

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
    _attackText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatHP_Copy001");
    _speedText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatHP_Copy001_Copy001");
    _defenseValueText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatHP_Copy001_Copy001_Copy001");
    _criticalRateText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatHP_Copy001_Copy001_Copy001_Copy001");


    return S_OK;
}

void CharacterStatus_Panel::Refresh_CombatStats()
{
    Shared<Player> player = CLIENT->Get_ActiveHero();
    CHECK_NULL(player);

    Shared<CombatStatCom> combatStat = player->Get_Component<CombatStatCom>();
    CHECK_NULL(combatStat);

    const CombatStatCom::CombatStatInfo& statInfo = combatStat->Get_StatInfo();

    Set_UIntText(_levelText, statInfo.level);
    Set_UIntText(_statPointText, statInfo.statPoint);

    Set_IntText(_vitalityText, statInfo.vitality);
    Set_IntText(_mightText, statInfo.might);
    Set_IntText(_agilityText, statInfo.agility);
    Set_IntText(_defenseStatText, statInfo.defense);
    Set_IntText(_luckText, statInfo.luck);
    
    Set_HpText(_hpText, statInfo.hp, statInfo.maxHp);
    Set_IntText(_attackText, static_cast<int32>(statInfo.attack));
    Set_IntText(_speedText, static_cast<int32>(statInfo.speed));
    Set_IntText(_defenseValueText, static_cast<int32>(statInfo.defenseValue));
    Set_PercentText(_criticalRateText, statInfo.criticalRate);
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
