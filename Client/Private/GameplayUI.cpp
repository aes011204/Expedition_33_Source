#include "GameplayUI.h"
#include "GameInstance.h"
#include "CharacterStatus_Panel.h"
#include "SkillTree_Panel.h"
#include "ChangeWeapon_Panel.h"
#include "UI_BattleTransition.h"
#include "SavePoint_Panel.h"
#include "UI_BattleCommandHUD.h"
#include "ClientInstance.h"
#include "BattleController.h"

HRESULT GameplayUI::Initialize(uint32 levelIndex)
{
    _characterStatusPanel = CharacterStatus_Panel::Create(levelIndex);
    CHECK_NULL(_characterStatusPanel, E_FAIL);

    _skillTreePanel = SkillTree_Panel::Create(levelIndex);
    CHECK_NULL(_skillTreePanel, E_FAIL);

    _changeWeaponPanel = ChangeWeapon_Panel::Create(levelIndex);
    CHECK_NULL(_changeWeaponPanel, E_FAIL);
    _battleTransition = UI_BattleTransition::Create(levelIndex);
    CHECK_NULL(_battleTransition, E_FAIL);

    _savePointPanel = SavePoint_Panel::Create(levelIndex);
    CHECK_NULL(_savePointPanel, E_FAIL);

    _battleCommandHUD = UI_BattleCommandHUD::Create(levelIndex);
    CHECK_NULL(_battleCommandHUD, E_FAIL);

    return S_OK;
}

void GameplayUI::Update(float timeDelta)
{
    Handle_TabInput(); // Tab은 root 메뉴인 SavePoint 진입/종료만 담당

    if (_savePointPanel)
        _savePointPanel->Update(timeDelta); 

    if (_characterStatusPanel)
        _characterStatusPanel->Update(timeDelta);

    if (_skillTreePanel)
        _skillTreePanel->Update(timeDelta);

    if (_changeWeaponPanel)
        _changeWeaponPanel->Update(timeDelta);

    if (_savePointPanel && _savePointPanel->Is_Open())
    {
        const SavePoint_Panel::SavePointAction action = _savePointPanel->Consume_LastAction();
        Handle_SavePointAction(action);
    }

    Handle_BackInput();

    Set_BattleCommandHUD(); // 배틀 현재 턴/FSM 상태를 읽어서 HUD 열기/닫기/강조 동기화

    if (_battleCommandHUD)
        _battleCommandHUD->Update(timeDelta);

    if (_battleTransition)
        _battleTransition->Update(timeDelta);

    GAME->Print(L"menuStack 사이즈 : {}", _menuStack.size(), duration(0), color(Vec4(1.f, 0.f, 0.f, 1.f)));
}

void GameplayUI::Play_BattleEnterTransition()
{
    if (_battleTransition)
        _battleTransition->Play_BattleEnter();
}

void GameplayUI::Stop_BattleEnterTransition()
{
    if (_battleTransition)
        _battleTransition->Stop();
}

bool GameplayUI::Is_BattleEnterTransitionPlaying() const
{
    return _battleTransition && _battleTransition->Is_Playing();
}

bool GameplayUI::Is_AnyMenuOpen() const
{
    return !_menuStack.empty();
}

GameplayUI::GameplayMenu GameplayUI::Get_CurrentMenu() const
{
    if (_menuStack.empty())
        return GameplayMenu::None;

    return _menuStack.back();
}

void GameplayUI::Handle_SavePointAction(SavePoint_Panel::SavePointAction action)
{
    switch (action)
    {
    case SavePoint_Panel::SavePointAction::Rest:
        Close_AllMenus(); // 현재는 SavePoint 닫기만. 나중에 Rest 처리 연결
        break;

    case SavePoint_Panel::SavePointAction::UpgradeStat:
        Push_Menu(GameplayMenu::CharacterStatus);   // SavePoint 위에 CharacterStatus push
        break;

    case SavePoint_Panel::SavePointAction::UpgradeSkill:
        Push_Menu(GameplayMenu::SkillTree);         // SavePoint 위에 SkillTree push
        break;

    case SavePoint_Panel::SavePointAction::UpgradeWeapon:
        Push_Menu(GameplayMenu::WeaponUpgrade);     // SavePoint 위에 WeaponUpgrade push
        break;

    case SavePoint_Panel::SavePointAction::None:
    default:
        break;
    }
}

void GameplayUI::Handle_TabInput()
{
    if (!GAME->KeyDown(KEY_TYPE::TAB))
        return;

    const GameplayMenu currentMenu = Get_CurrentMenu();

    if (currentMenu == GameplayMenu::None)
    {
        Push_Menu(GameplayMenu::SavePoint);
        return;
    }

    if (currentMenu == GameplayMenu::SavePoint)
    {
        // SavePoint에서 Tab은 전체 종료 -> 나중에는 깃발에서 E누르면 켜지는거로 변경 예정
        Close_AllMenus();
        return;
    }
}

void GameplayUI::Handle_BackInput()
{
    if (!GAME->KeyDown(KEY_TYPE::ESCAPE))
        return;

    if (_menuStack.empty())
        return;

    Pop_Menu();
}

void GameplayUI::Push_Menu(GameplayMenu menu)
{
    if (menu == GameplayMenu::None)
        return;

    const GameplayMenu currentMenu = Get_CurrentMenu();

    if (currentMenu == menu)
        return; // 같은 메뉴 중복 방지

    if (currentMenu != GameplayMenu::None)
    {
        Close_Menu(currentMenu); // 현재 top 메뉴를 먼저 닫는다
    }

    _menuStack.push_back(menu);
    Open_Menu(menu); // 새 top 메뉴 오픈
}

void GameplayUI::Pop_Menu()
{
    if (_menuStack.empty())
        return;

    const GameplayMenu currentMenu = _menuStack.back();
    Close_Menu(currentMenu); // 현재 top 메뉴 종료
    _menuStack.pop_back();

    if (_menuStack.empty())
        return; 

    Open_Menu(_menuStack.back());
}

void GameplayUI::Close_AllMenus()
{
    while (!_menuStack.empty())
    {
        Close_Menu(_menuStack.back());
        _menuStack.pop_back();
    }
}

void GameplayUI::Open_Menu(GameplayMenu menu)
{
    switch (menu)
    {
    case GameplayMenu::SavePoint:
        if (_savePointPanel)
            _savePointPanel->Open();
        break;

    case GameplayMenu::CharacterStatus:
        if (_characterStatusPanel)
            _characterStatusPanel->Open();
        break;

    case GameplayMenu::SkillTree:
        if (_skillTreePanel)
            _skillTreePanel->Open();
        break;

    case GameplayMenu::WeaponUpgrade:
        if (_changeWeaponPanel)
            _changeWeaponPanel->Open();
        break;

    case GameplayMenu::None:
    default:
        break;
    }
}

void GameplayUI::Close_Menu(GameplayMenu menu)
{
    switch (menu)
    {
    case GameplayMenu::SavePoint:
        if (_savePointPanel)
            _savePointPanel->Close();
        break;

    case GameplayMenu::CharacterStatus:
        if (_characterStatusPanel)
            _characterStatusPanel->Close();
        break;

    case GameplayMenu::SkillTree:
        if (_skillTreePanel)
            _skillTreePanel->Close();
        break;

    case GameplayMenu::WeaponUpgrade:
        if (_changeWeaponPanel)
            _changeWeaponPanel->Close();
        break;

    case GameplayMenu::None:
    default:
        break;
    }
}

void GameplayUI::Set_BattleCommandHUD()
{
    if (_battleCommandHUD == nullptr)
        return;

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _battleCommandHUD->Close(); // 전투가 아니면 HUD 숨김
        return;
    }

    if (!battleController->Is_PlayerTurnActive())
    {
        _battleCommandHUD->Close();
        return;
    }

    _battleCommandHUD->Open(); // 플레이어 턴일 때만 HUD 표시
    _battleCommandHUD->Refresh_ForState(battleController->Get_CurrentTurnState());
}

Unique<GameplayUI> GameplayUI::Create(uint32 levelIndex)
{
    auto instance = make_unique<GameplayUI>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : GameplayUI");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void GameplayUI::Free()
{
    auto Release_Panel = [](auto& panel)
        {
            if (panel)
            {
                panel->Free();
                panel.reset();
            }
        };

    Release_Panel(_savePointPanel);
    Release_Panel(_characterStatusPanel);
    Release_Panel(_skillTreePanel);
    Release_Panel(_changeWeaponPanel);
    Release_Panel(_battleTransition);
    Release_Panel(_battleCommandHUD);

    Base::Free();
}
