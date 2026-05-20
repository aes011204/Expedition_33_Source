#include "GameplayUI.h"
#include "GameInstance.h"
#include "CharacterStatus_Panel.h"
#include "SkillTree_Panel.h"
#include "UI_BattleTransition.h"
#include "SavePoint_Panel.h"
#include "UI_BattleCommandHUD.h"
#include "ClientInstance.h"
#include "BattleController.h"
#include "CombatStatCom.h"
#include "UI_MonsterHpBar.h"
#include "Character.h"

HRESULT GameplayUI::Initialize(uint32 levelIndex)
{
    _characterStatusPanel = CharacterStatus_Panel::Create(levelIndex);
    CHECK_NULL(_characterStatusPanel, E_FAIL);

    _skillTreePanel = SkillTree_Panel::Create(levelIndex);
    CHECK_NULL(_skillTreePanel, E_FAIL);

    _battleTransition = UI_BattleTransition::Create(levelIndex);
    CHECK_NULL(_battleTransition, E_FAIL);

    _savePointPanel = SavePoint_Panel::Create(levelIndex);
    CHECK_NULL(_savePointPanel, E_FAIL);

    _battleCommandHUD = UI_BattleCommandHUD::Create(levelIndex);
    CHECK_NULL(_battleCommandHUD, E_FAIL);

    _monsterHpBar = UI_MonsterHpBar::Create(levelIndex);
    CHECK_NULL(_monsterHpBar, E_FAIL);

    Bind_BattleDelegates();

    return S_OK;
}

void GameplayUI::Update(float timeDelta)
{
    // World UI
    Update_WorldUI(timeDelta);
    Update_BattleUI(timeDelta);
}

void GameplayUI::Update_WorldUI(float timeDelta)
{
    Handle_TabInput(); // Tab은 root 메뉴인 SavePoint 진입/종료만 담당

    if (_savePointPanel)
        _savePointPanel->Update(timeDelta); 

    if (_characterStatusPanel)
        _characterStatusPanel->Update(timeDelta);

    if (_skillTreePanel)
    {
        _skillTreePanel->Update(timeDelta);
    }

    if (_savePointPanel && _savePointPanel->Is_Open())
    {
        const SavePoint_Panel::SavePointAction action = _savePointPanel->Consume_LastAction();
        Handle_SavePointAction(action);
    }

    Handle_BackInput();
}

void GameplayUI::Update_BattleUI(float timeDelta)
{
    Update_BattleCommandHUD(); // 배틀 현재 턴/FSM 상태를 읽어서 HUD 열기/닫기/강조 동기화
    Update_MonsterHpBar();

    if (_battleCommandHUD)
        _battleCommandHUD->Update(timeDelta);

    if (_monsterHpBar)
        _monsterHpBar->Update(timeDelta);

    if (_battleTransition)
        _battleTransition->Update(timeDelta);
}

void GameplayUI::Play_BattleEnterTransition()
{
    if (_battleTransition)
        _battleTransition->Play_BattleEnter();
}

void GameplayUI::Stop_BattleEnterTransition()
{
    if (_battleTransition)
        _battleTransition->Close();
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
        // TODO: WeaponUpgrade_Panel 추가 후 연결
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
        // TODO: WeaponUpgrade_Panel 추가 후 연결
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
        // TODO: WeaponUpgrade_Panel 추가 후 연결
        break;

    case GameplayMenu::None:
    default:
        break;
    }
}

void GameplayUI::Bind_BattleDelegates()
{
    _battleController = CLIENT->Get_BattleController();
    CHECK_NULL(_battleController);

    _battleStartedHandle =
        _battleController->OnBattleStarted.Add(this, &GameplayUI::Handle_BattleStarted); 

    _battleEndedHandle =
        _battleController->OnBattleEnded.Add(this, &GameplayUI::Handle_BattleEnded);

    _turnStartedHandle =
        _battleController->OnTurnStarted.Add(this, &GameplayUI::Handle_TurnStarted);

    _turnEndedHandle =
        _battleController->OnTurnEnded.Add(this, &GameplayUI::Handle_TurnEnded);

    _targetPreviewChangedHandle =
        _battleController->OnTargetPreviewChanged.Add(this, &GameplayUI::Handle_TargetPreviewChanged);
}

void GameplayUI::Unbind_BattleDelegates()
{
    CHECK_NULL(_battleController);

    if (_battleStartedHandle.IsValid())
        _battleController->OnBattleStarted.Remove(_battleStartedHandle);

    if (_battleEndedHandle.IsValid())
        _battleController->OnBattleEnded.Remove(_battleEndedHandle);

    if (_turnStartedHandle.IsValid())
        _battleController->OnTurnStarted.Remove(_turnStartedHandle);

    if (_turnEndedHandle.IsValid())
        _battleController->OnTurnEnded.Remove(_turnEndedHandle);

    if (_targetPreviewChangedHandle.IsValid())
        _battleController->OnTargetPreviewChanged.Remove(_targetPreviewChangedHandle);

    _battleStartedHandle.Reset();
    _battleEndedHandle.Reset();
    _turnStartedHandle.Reset();
    _turnEndedHandle.Reset();
    _targetPreviewChangedHandle.Reset();

    _battleController.reset();
}

void GameplayUI::Handle_BattleStarted(uint32 fieldId, bool isFirstHero)
{
    // 아직 안씀
    UNREFERENCED_PARAMETER(fieldId);
    UNREFERENCED_PARAMETER(isFirstHero);

    _isBattleUiActive = true;
    _isPlayerTurnUiActive = false;
    _hasTargetPreviewSnapshot = false;
    _targetPreviewSnapshot = {};

    if (_battleCommandHUD)
        _battleCommandHUD->Close(); // 전투 시작 직후에는 아직 BattleCommand UI 안보여줌

    if (_monsterHpBar)
        _monsterHpBar->Close();
}

void GameplayUI::Handle_BattleEnded(EBattleEndResult result)
{
    UNREFERENCED_PARAMETER(result);

    _isBattleUiActive = false;      // 전투 끝나면 battle UI 전체 비활성
    _isPlayerTurnUiActive = false;
    _hasTargetPreviewSnapshot = false;
    _targetPreviewSnapshot = {};

    if (_battleCommandHUD)
        _battleCommandHUD->Close();

    if (_monsterHpBar)
        _monsterHpBar->Close();
}

void GameplayUI::Handle_TurnStarted(const BattleChar& actor, BattleTurnState state)
{
    UNREFERENCED_PARAMETER(state);

    // 플레이어 턴인지
    _isPlayerTurnUiActive =
        (actor.statCom != nullptr && actor.statCom->Is_Player());

    if (_battleCommandHUD)
    {
        if (_isPlayerTurnUiActive)
            _battleCommandHUD->Open();
        else
            _battleCommandHUD->Close();
    }
}

void GameplayUI::Handle_TurnEnded(const BattleChar& actor)
{
    UNREFERENCED_PARAMETER(actor);

    _isPlayerTurnUiActive = false;

    if (_battleCommandHUD)
        _battleCommandHUD->Close();

    if (_monsterHpBar)
        _monsterHpBar->Close();
}

void GameplayUI::Handle_TargetPreviewChanged(const BattleChar* target)
{
    if (target == nullptr || target->actor == nullptr || target->statCom == nullptr)
    {
        _targetPreviewSnapshot = {};
        _hasTargetPreviewSnapshot = false;

        if (_monsterHpBar)
            _monsterHpBar->Close();

        return;
    }

    _targetPreviewSnapshot = *target;
    _hasTargetPreviewSnapshot = true;
}

void GameplayUI::Update_MonsterHpBar()
{
    if (_monsterHpBar == nullptr)
        return;

    if (!_isBattleUiActive || !_hasTargetPreviewSnapshot)
    {
        _monsterHpBar->Close();
        return;
    }

    if (_targetPreviewSnapshot.actor == nullptr || _targetPreviewSnapshot.statCom == nullptr)
    {
        _monsterHpBar->Close();
        return;
    }

    if (_targetPreviewSnapshot.statCom->Get_AliveStatus() != EAliveStatus::Alive)
    {
        _monsterHpBar->Close(); // 죽은 놈
        return;
    }

    Vec3 worldPos{};
    if (!Try_GetTargetHpBarWorldPos(_targetPreviewSnapshot, worldPos))
    {
        _monsterHpBar->Close();
        return;
    }

    _monsterHpBar->Open();
    _monsterHpBar->Set_TargetWorldPosition(worldPos); 
    _monsterHpBar->Set_Hp(                         
        _targetPreviewSnapshot.statCom->Get_CurHp(),
        _targetPreviewSnapshot.statCom->Get_StatInfo().maxHp);
}

bool GameplayUI::Try_GetTargetHpBarWorldPos(const BattleChar& target, Vec3& outWorldPos) const
{
    if (target.actor == nullptr || target.actor->Get_Transform() == nullptr)
        return false;

    outWorldPos = target.actor->Get_Transform()->Get_WorldPosition();
    outWorldPos.y += 1.5f; // 임시 테스트 위치
    return true;
}

void GameplayUI::Update_BattleCommandHUD()
{
    if (_battleCommandHUD == nullptr)
        return;

    if (!_isBattleUiActive || !_isPlayerTurnUiActive)
    {
        _battleCommandHUD->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _battleCommandHUD->Close();
        return;
    }

    _battleCommandHUD->Refresh_ForState(battleController->Get_CurrentTurnState());
}

Shared<GameplayUI> GameplayUI::Create(uint32 levelIndex)
{
    auto instance = make_shared<GameplayUI>();

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
    Release_Panel(_battleTransition);
    Release_Panel(_battleCommandHUD);
    Release_Panel(_monsterHpBar);

    Base::Free();
}
