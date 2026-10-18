#include "UI_ArenaCharacterSelect.h"

#include "Client_Constants.h"
#include "ClientInstance.h"
#include "GameInstance.h"
#include "PlayerStatCom.h"
#include "Player_WorldBase.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace  UI_Utils;

HRESULT UI_ArenaCharacterSelect::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately(false);

    return S_OK;
}

void UI_ArenaCharacterSelect::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!_isOpen)
        return;

    Refresh_PlayerStats();
    Handle_ButtonInput();
}

void UI_ArenaCharacterSelect::Open()
{
    if (_root == nullptr)
        return;

    if (_isOpen)
        return;

    _lastAction = Action::None;
    Reset_ButtonInputLatch();
    Refresh_PlayerStats();

    UI_RuntimePanelBase::Open();

    GAME->Unlock_Mouse();
    GAME->Set_GameplayInputMode(GameplayInputMode::UIOnly);

    if (_luneButton)
        _luneButton->Suppress_HoverStateAnimationUntilMouseMove();
    if (_gustaveButton)
        _gustaveButton->Suppress_HoverStateAnimationUntilMouseMove();
    if (_maelleButton)
        _maelleButton->Suppress_HoverStateAnimationUntilMouseMove();
}

void UI_ArenaCharacterSelect::Close()
{
    Close_Immediately(true);
}

void UI_ArenaCharacterSelect::Close_Immediately(bool restoreWorldControl)
{
    _lastAction = Action::None;
    Reset_ButtonInputLatch();

    UI_RuntimePanelBase::Close();

    if (restoreWorldControl)
    {
        GAME->Set_GameplayInputMode(GameplayInputMode::Normal);
        GAME->Lock_Mouse();
    }
}

void UI_ArenaCharacterSelect::Close_ForMenuTransition()
{
    Close_Immediately(false);
}

UI_ArenaCharacterSelect::Action UI_ArenaCharacterSelect::Consume_LastAction()
{
    const Action action = _lastAction;
    _lastAction = Action::None;

    return action;
}

HRESULT UI_ArenaCharacterSelect::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Arena_Character_Select_Path;
    desc.prototypeTagPrefix = L"ArenaCharacterSelect";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = UI_Utils::Find_UI(_uiByName, L"Panel_Arena_Character_Select");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_ArenaCharacterSelect::Bind_UI()
{
    _luneButton = UI_Utils::Find_TypedUI<UI_Button>(
        _uiByName, L"Button_Arena_Character_Select_Lune", "ArenaCharacterSelect");
    _gustaveButton = UI_Utils::Find_TypedUI<UI_Button>(
        _uiByName, L"Button_Arena_Character_Select_Gustave", "ArenaCharacterSelect");
    _maelleButton = UI_Utils::Find_TypedUI<UI_Button>(
        _uiByName, L"Button_Arena_Character_Select_Maelle", "ArenaCharacterSelect");
    _backButton = UI_Utils::Find_TypedUI<UI_Button>(
        _uiByName, L"Button_Arena_Character_Select_Back", "ArenaCharacterSelect");

    CHECK_NULL(_luneButton, E_FAIL);
    CHECK_NULL(_gustaveButton, E_FAIL);
    CHECK_NULL(_maelleButton, E_FAIL);
    CHECK_NULL(_backButton, E_FAIL);

    Bind_PlayerStatSlot(L"Gustave", 0);
    Bind_PlayerStatSlot(L"Lune", 1);
    Bind_PlayerStatSlot(L"Maelle", 2);

    for (uint32 i = 0; i < kPlayerStatSlotCount; ++i)
    {
        CHECK_NULL(_playerStatSlots[i].levelText, E_FAIL);
        CHECK_NULL(_playerStatSlots[i].currentHpText, E_FAIL);
        CHECK_NULL(_playerStatSlots[i].maxHpText, E_FAIL);
        CHECK_NULL(_playerStatSlots[i].hpGauge, E_FAIL);

        _playerStatSlots[i].hpGauge->Set_UseFillRatio(true);
        _playerStatSlots[i].hpGauge->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);
    }

    return S_OK;
}

void UI_ArenaCharacterSelect::Refresh_PlayerStats()
{
    Set_PlayerStatSlot(HeroId::Gustave, 0);
    Set_PlayerStatSlot(HeroId::Lune, 1);
    Set_PlayerStatSlot(HeroId::Maelle, 2);
}

void UI_ArenaCharacterSelect::Bind_PlayerStatSlot(const wstring& uiName, uint32 slotIndex)
{
    if (slotIndex >= kPlayerStatSlotCount)
        return;

    PlayerStatSlot& slot = _playerStatSlots[slotIndex];

    slot.levelText = UI_Utils::Find_TypedUI<UI_Text>(
        _uiByName, L"Text_Arena_Character_Select_" + uiName + L"_Level_Value", "ArenaCharacterSelect");
    slot.currentHpText = UI_Utils::Find_TypedUI<UI_Text>(
        _uiByName, L"Text_Arena_Character_Select_" + uiName + L"_Current_HP", "ArenaCharacterSelect");
    slot.maxHpText = UI_Utils::Find_TypedUI<UI_Text>(
        _uiByName, L"Text_Arena_Character_Select_" + uiName + L"_Max_HP", "ArenaCharacterSelect");
    slot.hpGauge = UI_Utils::Find_TypedUI<UI_Image>(
        _uiByName, L"Image_Arena_Character_Select_" + uiName + L"_HP_Gauge", "ArenaCharacterSelect");
}

void UI_ArenaCharacterSelect::Set_PlayerStatSlot(HeroId heroId, uint32 slotIndex)
{
    if (slotIndex >= kPlayerStatSlotCount)
        return;

    const Shared<Player_WorldBase> player = CLIENT->Find_Hero(heroId);
    const Shared<PlayerStatCom> playerStat = player ? player->Get_Component<PlayerStatCom>() : nullptr;

    if (playerStat == nullptr)
    {
        Apply_PlayerStatSlot(slotIndex, 0, 0.f, 0.f);
        return;
    }

    const PlayerStatCom::PRuntimeCombatStat& statInfo = playerStat->Get_PlayerRuntimeStat();
    Apply_PlayerStatSlot(slotIndex, statInfo.CurLevel, statInfo.CurHp, statInfo.CurMaxHp);
}

void UI_ArenaCharacterSelect::Apply_PlayerStatSlot(uint32 slotIndex, uint32 level, float curHp, float maxHp)
{
    if (slotIndex >= kPlayerStatSlotCount)
        return;

    PlayerStatSlot& slot = _playerStatSlots[slotIndex];

    const float displayMaxHp = max(maxHp, 0.f);
    const float gaugeMaxHp = max(displayMaxHp, 1.f);
    const float displayCurHp = displayMaxHp > 0.f ? clamp(curHp, 0.f, displayMaxHp) : 0.f;

    if (slot.levelText)
        slot.levelText->Set_Text(to_wstring(level));

    if (slot.currentHpText)
        slot.currentHpText->Set_Text(to_wstring(static_cast<int32>(displayCurHp)));

    if (slot.maxHpText)
        slot.maxHpText->Set_Text(to_wstring(static_cast<int32>(displayMaxHp)));

    if (slot.hpGauge)
        slot.hpGauge->Set_FillRatio(displayCurHp / gaugeMaxHp);
}

void UI_ArenaCharacterSelect::Handle_ButtonInput()
{
    if (_gustaveButton && _gustaveButton->Was_Clicked())
    {
        _lastAction = Action::SelectGustave;
        return;
    }

    if (_luneButton && _luneButton->Was_Clicked())
    {
        _lastAction = Action::SelectLune;
        return;
    }

    if (_maelleButton && _maelleButton->Was_Clicked())
    {
        _lastAction = Action::SelectMaelle;
        return;
    }

    if (_backButton && _backButton->Was_Clicked())
    {
        _lastAction = Action::Back;
        return;
    }
}

void UI_ArenaCharacterSelect::Reset_ButtonInputLatch()
{
    if (_gustaveButton)
        _gustaveButton->Update(0.f); // 닫히는 프레임에 남은 Was_Clicked를 다음 Open으로 넘기지 않는다.
    if (_luneButton)
        _luneButton->Update(0.f);
    if (_maelleButton)
        _maelleButton->Update(0.f);
    if (_backButton)
        _backButton->Update(0.f);
}

Unique<UI_ArenaCharacterSelect> UI_ArenaCharacterSelect::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_ArenaCharacterSelect>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_ArenaCharacterSelect");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_ArenaCharacterSelect::Free()
{
    if (_root)
        Close_Immediately(false);

    _gustaveButton.reset();
    _luneButton.reset();
    _maelleButton.reset();
    _backButton.reset();
    for (PlayerStatSlot& slot : _playerStatSlots)
    {
        slot.levelText.reset();
        slot.currentHpText.reset();
        slot.maxHpText.reset();
        slot.hpGauge.reset();
    }
    _root.reset();
    _uiByName.clear();

    Base::Free();
}
