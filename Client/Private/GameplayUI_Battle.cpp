#include "GameplayUI_Battle.h"

#include "BattleController.h"
#include "Character.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"
#include "DamageSystem.h"

#include "UI_BattleCommandHUD.h"
#include "UI_BattleDefense.h"
#include "UI_BattleSystemHUD.h"
#include "UI_BattleSystem_TurnOrder.h"
#include "UI_BattleTransition.h"
#include "UI_CancelAim.h"
#include "UI_ChoiceTarget.h"
#include "UI_MonsterHpBar.h"
#include "UI_CrossHair.h"
#include "UI_DamageFont.h"

GameplayUI_Battle::~GameplayUI_Battle() = default;

HRESULT GameplayUI_Battle::Initialize(uint32 levelIndex)
{
    auto Create_UI = [levelIndex](auto& ui, auto& createFn) -> HRESULT
        {
            ui = createFn(levelIndex);
            CHECK_NULL(ui, E_FAIL);
            return S_OK;
        };

    // 람다 의미없는거같기도 하고
    CHECK_FAILED(Create_UI(_battleTransition, UI_BattleTransition::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleCommandHUD, UI_BattleCommandHUD::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleSystemHUD, UI_BattleSystemHUD::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_monsterHpBar, UI_MonsterHpBar::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_crossHair, UI_CrossHair::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleDefense, UI_BattleDefense::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_cancelAim, UI_CancelAim::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_choiceTarget, UI_ChoiceTarget::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_turnOrder, UI_BattleSystem_TurnOrder::Create), E_FAIL);

    for (auto& damageFont : _damageFonts)
    {
        damageFont = UI_DamageFont::Create(levelIndex);
        CHECK_NULL(damageFont, E_FAIL);
    }

    Bind_BattleDelegates();

    return S_OK;
}

void GameplayUI_Battle::Update(float timeDelta)
{
    Update_BattleCommandHUD(timeDelta); // 플레이어 전투 입력 허드
    Update_BattleSystemHUD(timeDelta);  // 파티 portrait HP/AP HUD
    Update_MonsterHpBar(timeDelta);     // 몬스터 체력
    Update_CrossHair(timeDelta);        // Free Aim 크로스헤어
    Update_BattleDefense(timeDelta);    // 몬스터 턴 방어 (좌측 하단)
    Update_CancelAim(timeDelta);        // Free Aim 취소
    Update_ChoiceTarget(timeDelta);     // 타겟 선택 UI (F 대상 선택, ESC 뒤로)
    Update_TurnOrder(timeDelta);        // 좌측상단 턴 UI
    Update_DamageFonts(timeDelta);      // 데미지 폰트

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

    _actionStartedHandle =
        _battleController->OnActionStarted.Add(this, &GameplayUI_Battle::Handle_ActionStarted);

    _damageAppliedHandle =
        DamageSystem::OnDamageApplied.Add(this, &GameplayUI_Battle::Handle_DamageApplied);
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

    if (_actionStartedHandle.IsValid())
        _battleController->OnActionStarted.Remove(_actionStartedHandle);

    if (_damageAppliedHandle.IsValid())
        DamageSystem::OnDamageApplied.Remove(_damageAppliedHandle);

    _battleStartedHandle.Reset();
    _battleEndedHandle.Reset();
    _turnStartedHandle.Reset();
    _turnEndedHandle.Reset();
    _targetPreviewChangedHandle.Reset();
    _actionStartedHandle.Reset();
    _damageAppliedHandle.Reset();

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
    _hasCurrentPlayerTurnActor = false;
    _currentPlayerTurnActor = {};

    UI_RuntimePanelBase* panelsToClose[] =
    {
        // 턴 시작 시 닫아야할 UI들이 있다면 여기에 추가
        _battleCommandHUD.get(),
        _battleSystemHUD.get(),
        _monsterHpBar.get(),
        _crossHair.get(),
        _battleDefense.get(),
        _cancelAim.get(),
        _choiceTarget.get(),
    };

    for (UI_RuntimePanelBase* panel : panelsToClose)
    {
        if (panel)
            panel->Close();
    }

    // 이전 전투에서 남은 배틀 슬롯 정리
    for (auto& damageFont : _damageFonts)
    {
        if (damageFont)
            damageFont->Close();
    }

    Play_BattleEnterTransition(); // 전투 진입 연출은 battle UI 쪽에서 시작한다.
}

void GameplayUI_Battle::Handle_BattleEnded(EBattleEndResult result)
{
    UNREFERENCED_PARAMETER(result);

    _isBattleUiActive = false;      // 전투 끝나면 battle UI 전체 비활성
    _isPlayerTurnUiActive = false;
    _hasTargetPreviewSnapshot = false;
    _targetPreviewSnapshot = {};
    _hasCurrentPlayerTurnActor = false;
    _currentPlayerTurnActor = {};

    UI_RuntimePanelBase* panelsToClose[] =
    {
        // 턴 시작 시 닫아야할 UI들이 있다면 여기에 추가
        _battleCommandHUD.get(),
        _battleSystemHUD.get(),
        _monsterHpBar.get(),
        _crossHair.get(),
        _battleDefense.get(),
        _cancelAim.get(),
        _choiceTarget.get(),
    };

    for (auto* panel : panelsToClose)
    {
        if (panel)
            panel->Close();
    }

    for (auto& damageFont : _damageFonts)
    {
        if (damageFont)
            damageFont->Close();
    }

    Stop_BattleEnterTransition(); // 전투 종료 시 남아있는 transition도 같이 정리한다.
}

void GameplayUI_Battle::Handle_TurnStarted(const BattleChar& actor, BattleTurnState state)
{
    UNREFERENCED_PARAMETER(state);

    // 플레이어 턴인지
    _isPlayerTurnUiActive =
        (actor.statCom != nullptr && actor.statCom->Is_Player());

    if (_isPlayerTurnUiActive)
    {
        _currentPlayerTurnActor = actor;
        _hasCurrentPlayerTurnActor = true;
    }
    else
    {
        _currentPlayerTurnActor = {};
        _hasCurrentPlayerTurnActor = false;
    }

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
    _hasCurrentPlayerTurnActor = false;
    _currentPlayerTurnActor = {};

    UI_RuntimePanelBase* panelsToClose[] =
    {
        _battleCommandHUD.get(),
        _monsterHpBar.get(),
        _crossHair.get(),
        _battleDefense.get(),
        _cancelAim.get(),
        _choiceTarget.get(),
    };

    for (auto* panel : panelsToClose)
    {
        if (panel)
            panel->Close();
    }
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

void GameplayUI_Battle::Handle_ActionStarted(const Action& action)
{
    UNREFERENCED_PARAMETER(action);

    if (_choiceTarget)
        _choiceTarget->Close();
}

void GameplayUI_Battle::Handle_DamageApplied(const BattleChar& target, float damage)
{
    if (!_isBattleUiActive)
        return;

    if (damage <= 0.f)
        return;

    Vec3 worldPos{};
    if (!Try_GetTargetHpBarWorldPos(target, worldPos))
        return;

    UI_DamageFont* selectedSlot = nullptr;
    UI_DamageFont* oldestSlot = nullptr;
    float longestElapsed = -1.f;
    uint32 stackIndex = 0;

    for (auto& damageFont : _damageFonts)
    {
        if (!damageFont)
            continue;

        if (damageFont->Is_Playing())
        {
            ++stackIndex; // 이미 떠 있는 숫자 수를 오프셋 인덱스로 사용

            if (damageFont->Get_Elapsed() > longestElapsed)
            {
                longestElapsed = damageFont->Get_Elapsed();
                oldestSlot = damageFont.get();
            }

            continue;
        }

        if (selectedSlot == nullptr)
            selectedSlot = damageFont.get(); // 빈 슬롯을 우선 사용
    }

    if (selectedSlot == nullptr)
        selectedSlot = oldestSlot; // 모든 슬롯이 사용 중이면 가장 오래된 숫자를 재사용

    if (selectedSlot == nullptr)
        return;

    stackIndex = min(stackIndex, kDamageFontPoolSize - 1);
    selectedSlot->Play_DamageFont(worldPos, damage, stackIndex);


}

void GameplayUI_Battle::Update_BattleCommandHUD(float timeDelta)
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

    const BattleTurnState currentState = battleController->Get_CurrentTurnState();

    if (currentState != BattleTurnState::ChooseAction)
    {
        _battleCommandHUD->Close(); // 공격 중에는 커맨드 HUD 숨기기
        return;
    }

    Vec3 worldPos{};
    if (!Try_GetBattleCommandHudWorldPos(_currentPlayerTurnActor, worldPos))
    {
        _battleCommandHUD->Close();
        return;
    }

    _battleCommandHUD->Open();
    _battleCommandHUD->Set_TargetWorldPosition(worldPos);
    _battleCommandHUD->Refresh_ForState(battleController->Get_CurrentTurnState());

    _battleCommandHUD->Update(timeDelta);
}

void GameplayUI_Battle::Update_BattleSystemHUD(float timeDelta)
{
    if (_battleSystemHUD == nullptr)
        return;

    if (!_isBattleUiActive)
    {
        _battleSystemHUD->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _battleSystemHUD->Close();
        return;
    }

    _battleSystemHUD->Open(); // 전투 중에는 파티 portrait HUD를 계속 유지한다.

    _battleSystemHUD->Set_PartyBattleStat(PlayerCharacterType::Gustave, 0.f, 1.f, 0u, 1u);
    _battleSystemHUD->Set_PartyBattleStat(PlayerCharacterType::Lune, 0.f, 1.f, 0u, 1u);
    _battleSystemHUD->Set_PartyBattleStat(PlayerCharacterType::Maelle, 0.f, 1.f, 0u, 1u);

    const vector<BattleChar>& heroes = battleController->Get_BattleHeroes();
    for (const BattleChar& hero : heroes)
    {
        if (hero.statCom == nullptr)
            continue;

        _battleSystemHUD->Set_PartyBattleStat(
            hero.statCom->Get_PlayerType(),
            hero.statCom->Get_CurHp(),
            hero.statCom->Get_StatInfo().maxHp,
            hero.statCom->Get_CurAP(),
            hero.statCom->Get_MaxAP());
    }

    _battleSystemHUD->Update(timeDelta);
}

void GameplayUI_Battle::Update_MonsterHpBar(float timeDelta)
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

    _monsterHpBar->Update(timeDelta);

}

void GameplayUI_Battle::Update_CrossHair(float timeDelta)
{
    if (_crossHair == nullptr)
        return;

    if (!_isBattleUiActive || !_isPlayerTurnUiActive)
    {
        _crossHair->Close(); // 플레이어 턴 UI가 아닐 때는 Free Aim 상태가 남아 있어도 크로스헤어를 숨긴다.
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _crossHair->Close();
        return;
    }

    if (battleController->Get_CurrentTurnState() != BattleTurnState::FreeAimTargeting)
    {
        _crossHair->Close();
        return;
    }

    _crossHair->Open();
    _crossHair->Set_HitState(battleController->Get_TargetPreview() != nullptr);

    _crossHair->Update(timeDelta);
}

void GameplayUI_Battle::Update_BattleDefense(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (_battleDefense == nullptr)
        return;

    if (!_isBattleUiActive)
    {
        _battleDefense->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _battleDefense->Close();
        return;
    }

    const BattleChar& currentActor = battleController->Get_TurnAction()._curChar;
    if (currentActor.actor == nullptr || currentActor.statCom == nullptr)
    {
        _battleDefense->Close();
        return;
    }

    if (currentActor.statCom->Is_Player())
    {
        _battleDefense->Close(); // 플레이어 턴에는 Defense UI 숨기기
        return;
    }

    _battleDefense->Open(); // 몬스터 턴일 동안 유지
    _battleDefense->Update(timeDelta);

}

void GameplayUI_Battle::Update_CancelAim(float timeDelta)
{
    if (_cancelAim == nullptr)
        return;

    if (!_isBattleUiActive || !_isPlayerTurnUiActive)
    {
        _cancelAim->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _cancelAim->Close();
        return;
    }

    if (battleController->Get_CurrentTurnState() != BattleTurnState::FreeAimTargeting)
    {
        _cancelAim->Close();
        return;
    }

    _cancelAim->Open(); // Free Aim 중에는 크로스헤어, 캔슬 UI
    _cancelAim->Update(timeDelta);
}

void GameplayUI_Battle::Update_ChoiceTarget(float timeDelta)
{
    if (_choiceTarget == nullptr)
        return;

    if (!_isBattleUiActive || !_isPlayerTurnUiActive)
    {
        _choiceTarget->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _choiceTarget->Close();
        return;
    }

    if (battleController->Get_CurrentTurnState() != BattleTurnState::AttackTargeting)
    {
        _choiceTarget->Close();
        return;
    }

    _choiceTarget->Open();
    _choiceTarget->Update(timeDelta);
}

void GameplayUI_Battle::Update_TurnOrder(float timeDelta)
{
    if (_turnOrder == nullptr)
        return;

    if (!_isBattleUiActive || _battleController == nullptr)
    {
        _turnOrder->Clear_TurnOrderPreview();
        _turnOrder->Close();
        return;
    }

    _turnOrder->Open();
    _turnOrder->Set_TurnOrderPreview(Build_TurnOrderEntries());
    _turnOrder->Update(timeDelta);
}

void GameplayUI_Battle::Update_DamageFonts(float timeDelta)
{
    for (auto& damageFont : _damageFonts)
    {
        if (damageFont)
            damageFont->Update(timeDelta);
    }
}

bool GameplayUI_Battle::Try_GetTargetHpBarWorldPos(const BattleChar& target, Vec3& outWorldPos) const
{
    if (target.actor == nullptr || target.actor->Get_Transform() == nullptr)
        return false;

    outWorldPos = target.actor->Get_Transform()->Get_WorldPosition();
    outWorldPos.y += 1.5f; // 임시 테스트 위치
    return true;
}

bool GameplayUI_Battle::Try_GetBattleCommandHudWorldPos(const BattleChar& actor, Vec3& outWorldPos) const
{
    if (actor.actor == nullptr || actor.actor->Get_Transform() == nullptr)
        return false;

    outWorldPos = actor.actor->Get_Transform()->Get_WorldPosition();
    outWorldPos.y += 0.8f;

    return true;
}

vector<GameplayUI_Battle::TurnDesc> GameplayUI_Battle::Build_TurnOrderEntries() const
{
    vector<UI_BattleSystem_TurnOrder::TurnOrderEntryDesc> entries{};

    if (_battleController == nullptr)
        return entries;

    const vector<BattleChar> preview = _battleController->Get_TurnOrderPreview(8);

    for (const BattleChar& actor : preview)
    {
        UI_BattleSystem_TurnOrder::TurnOrderEntryDesc entry{};
        entry.portraitGuid = Resolve_TurnOrderPortraitGuid(actor);
        entry.isEnemy = actor.statCom != nullptr && !actor.statCom->Is_Player();
        entries.push_back(entry);
    }

    return entries;
}

string GameplayUI_Battle::Resolve_TurnOrderPortraitGuid(const BattleChar& actor)
{
    if (actor.statCom == nullptr)
        return {};

    // 프리펩에서 이미지 세팅
    return actor.statCom->Get_StatInfo().portraitGuid;
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
    Release_UI(_battleSystemHUD);
    Release_UI(_monsterHpBar);
    Release_UI(_crossHair);
    Release_UI(_battleDefense);
    Release_UI(_cancelAim);
    Release_UI(_choiceTarget);

    for (auto& damageFont : _damageFonts)
        Release_UI(damageFont);

    Base::Free();
}
