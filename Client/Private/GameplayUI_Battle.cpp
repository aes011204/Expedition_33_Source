#include "GameplayUI_Battle.h"

#include "BattleController.h"
#include "Character.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"
#include "UI_BattleCommandHUD.h"
#include "UI_BattleTransition.h"
#include "UI_MonsterHpBar.h"

GameplayUI_Battle::~GameplayUI_Battle() = default;

HRESULT GameplayUI_Battle::Initialize(uint32 levelIndex)
{
    _battleTransition = UI_BattleTransition::Create(levelIndex);
    CHECK_NULL(_battleTransition, E_FAIL);

    _battleCommandHUD = UI_BattleCommandHUD::Create(levelIndex);
    CHECK_NULL(_battleCommandHUD, E_FAIL);

    _monsterHpBar = UI_MonsterHpBar::Create(levelIndex);
    CHECK_NULL(_monsterHpBar, E_FAIL);

    Bind_BattleDelegates();

    return S_OK;
}

void GameplayUI_Battle::Update(float timeDelta)
{
    Update_BattleCommandHUD(); 
    Update_MonsterHpBar();

    if (_battleCommandHUD)
        _battleCommandHUD->Update(timeDelta);

    if (_monsterHpBar)
        _monsterHpBar->Update(timeDelta);

    if (_battleTransition)
        _battleTransition->Update(timeDelta);
}

void GameplayUI_Battle::Update_Input()
{
    
}

void GameplayUI_Battle::Play_BattleEnterTransition()
{
    if (_battleTransition)
        _battleTransition->Play_BattleEnter();
}

void GameplayUI_Battle::Stop_BattleEnterTransition()
{
    if (_battleTransition)
        _battleTransition->Close();
}

bool GameplayUI_Battle::Is_BattleEnterTransitionPlaying() const
{
    return _battleTransition && _battleTransition->Is_Playing();
}

void GameplayUI_Battle::Bind_BattleDelegates()
{
    _battleController = CLIENT->Get_BattleController();
    CHECK_NULL(_battleController);

    _battleStartedHandle =
        _battleController->OnBattleStarted.Add(this, &GameplayUI_Battle::Handle_BattleStarted);

    _battleEndedHandle =
        _battleController->OnBattleEnded.Add(this, &GameplayUI_Battle::Handle_BattleEnded);

    _turnStartedHandle =
        _battleController->OnTurnStarted.Add(this, &GameplayUI_Battle::Handle_TurnStarted);

    _turnEndedHandle =
        _battleController->OnTurnEnded.Add(this, &GameplayUI_Battle::Handle_TurnEnded);

    _targetPreviewChangedHandle =
        _battleController->OnTargetPreviewChanged.Add(this, &GameplayUI_Battle::Handle_TargetPreviewChanged);
}

void GameplayUI_Battle::Unbind_BattleDelegates()
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

void GameplayUI_Battle::Handle_BattleStarted(uint32 fieldId, bool isFirstHero)
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

    Play_BattleEnterTransition(); // 전투 진입 연출은 battle UI 쪽에서 시작한다.
}

void GameplayUI_Battle::Handle_BattleEnded(EBattleEndResult result)
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

    Stop_BattleEnterTransition(); // 전투 종료 시 남아있는 transition도 같이 정리한다.
}

void GameplayUI_Battle::Handle_TurnStarted(const BattleChar& actor, BattleTurnState state)
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

void GameplayUI_Battle::Handle_TurnEnded(const BattleChar& actor)
{
    UNREFERENCED_PARAMETER(actor);

    _isPlayerTurnUiActive = false;

    if (_battleCommandHUD)
        _battleCommandHUD->Close();

    if (_monsterHpBar)
        _monsterHpBar->Close();
}

void GameplayUI_Battle::Handle_TargetPreviewChanged(const BattleChar* target)
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

void GameplayUI_Battle::Update_MonsterHpBar()
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

bool GameplayUI_Battle::Try_GetTargetHpBarWorldPos(const BattleChar& target, Vec3& outWorldPos) const
{
    if (target.actor == nullptr || target.actor->Get_Transform() == nullptr)
        return false;

    outWorldPos = target.actor->Get_Transform()->Get_WorldPosition();
    outWorldPos.y += 1.5f; // 임시 테스트 위치
    return true;
}

void GameplayUI_Battle::Update_BattleCommandHUD()
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

Shared<GameplayUI_Battle> GameplayUI_Battle::Create(uint32 levelIndex)
{
    auto instance = make_shared<GameplayUI_Battle>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : GameplayUI_Battle");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void GameplayUI_Battle::Free()
{
    Unbind_BattleDelegates(); // BattleController가 살아있는 동안 dangling handle이 남지 않게 먼저 정리한다.

    auto Release_UI = [](auto& ui)
        {
            if (ui)
            {
                ui->Free();
                ui.reset();
            }
        };

    Release_UI(_battleTransition);
    Release_UI(_battleCommandHUD);
    Release_UI(_monsterHpBar);

    Base::Free();
}
