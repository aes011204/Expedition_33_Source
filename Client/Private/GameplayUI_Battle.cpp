#include "GameplayUI_Battle.h"

#include "BuffSystem.h"
#include "Character_BattleBase.h"
#include "Monster_BattleBase.h"
#include "BattleWorldUI_CommandCards.h"
#include "BattleWorldUI_ItemCards.h"
#include "BattleWorldUI_SkillCards.h"
#include "ClientInstance.h"
#include "Client_Constants.h"
#include "ClientScreenFxManager.h"
#include "CombatStatCom.h"
#include "DamageSystem.h"
#include "PlayerStatCom.h"
#include "ScreenFx_Manager.h"
#include "Skill_Gustave_Battle.h"
#include "Mon_WeakPoint.h"

#include "UI_AttackTooltip.h"
#include "UI_BattleCommandHUD.h"
#include "UI_BattleDefense.h"
#include "UI_BattleMechanism.h"
#include "UI_BattleSystemHUD.h"
#include "UI_BattleSystem_TurnOrder.h"
#include "UI_BattleTransition.h"
#include "UI_CancelAim.h"
#include "UI_ChoiceTarget.h"
#include "UI_MonsterHpBar.h"
#include "UI_BossHpBar.h"
#include "UI_PlayerHpBar.h"
#include "UI_CrossHair.h"
#include "UI_DamageFont.h"
#include "UI_DestroyShield.h"
#include "UI_EscapeSkipTurnBack.h"
#include "UI_ReactionResult.h"
#include "UI_BattleVictory.h"
#include "UI_BattleItemPanel.h"
#include "UI_BattleSkillPanel.h"
#include "UI_HealFont.h"
#include "UI_BattleAnnounce.h"
#include "UI_SuddenAttack.h"
#include "UI_AddAP.h"
#include "UI_QTE.h"
#include "UI_MarkEffect.h"
#include "MonsterStatCom.h"
#include "UI_BattleTutorial.h"
#include "BattleTutorialController.h"
#include "BattleResultRecoder.h"
#include "BattleSpawnRegistry.h"

#include "UI_Utils.h"

using namespace UI_Utils;

GameplayUI_Battle::~GameplayUI_Battle() = default;

namespace
{
    constexpr float kBattleSystemIntroDurationSec = 3.f;

    void Append_RuntimePanelEntry(
        vector<RuntimeUIEntry>& outEntries,
        const char* name,
        UI_RuntimePanelBase* panel)
    {
        if (panel == nullptr)
            return;

        Shared<UI_Base> root = panel->Get_EditorRootUI();
        if (root == nullptr)
            return;

        outEntries.push_back(
            {
                "Battle",
                name,
                root,
                panel,
                panel->Is_EditorPositionDrivenByCode()
            });
    }

    void Append_OpenedRuntimePanelEntry(
        vector<RuntimeUIEntry>& outEntries,
        const char* name,
        UI_RuntimePanelBase* panel)
    {
        if (panel == nullptr || !panel->Is_Open())
            return;

        Append_RuntimePanelEntry(outEntries, name, panel);
    }

    void Append_RuntimeWorldPlacementEntry(
        vector<RuntimeUIEntry>& outEntries,
        const char* name,
        IRuntimeWorldUI_EditorTarget* target)
    {
        if (target == nullptr)
            return;

        RuntimeUIEntry entry{};
        entry.category = "Battle";
        entry.name = name;
        entry.kind = RuntimeUIEntryKind::WorldPlacement;
        entry.worldPlacementTarget = target;
        outEntries.emplace_back(entry);
    }
    wstring Resolve_BattleVictoryPlayerName(PlayerCharacterType playerType)
    {
        switch (playerType)
        {
        case PlayerCharacterType::Gustave:
            return L"구스타브";
        case PlayerCharacterType::Lune:
            return L"루네";
        case PlayerCharacterType::Maelle:
            return L"마엘";
        default:
            return {};
        }
    }

HRESULT GameplayUI_Battle::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

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
    CHECK_FAILED(Create_UI(_playerHpBar, UI_PlayerHpBar::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_crossHair, UI_CrossHair::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleDefense, UI_BattleDefense::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_cancelAim, UI_CancelAim::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_choiceTarget, UI_ChoiceTarget::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_escapeSkipTurn, UI_EscapeSkipTurnBack::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_attackTooltip, UI_AttackTooltip::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_reactionResult, UI_ReactionResult::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleVictory, UI_BattleVictory::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_turnOrder, UI_BattleSystem_TurnOrder::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleMechanism, UI_BattleMechanism::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleItemPanel, UI_BattleItemPanel::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleSkillPanel, UI_BattleSkillPanel::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleAnnounce, UI_BattleAnnounce::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_suddenAttack, UI_SuddenAttack::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_qte, UI_QTE::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_markEffect, UI_MarkEffect::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleTutorial, UI_BattleTutorial::Create), E_FAIL);

    // 데미지 폰트 최대 8개
    for (auto& damageFont : _damageFonts)
    {
        damageFont = UI_DamageFont::Create(levelIndex);
        CHECK_NULL(damageFont, E_FAIL);
    }

    for (auto& destroyShield : _destroyShields)
    {
        destroyShield = UI_DestroyShield::Create(levelIndex);
        CHECK_NULL(destroyShield, E_FAIL);
    }

    // 힐 폰트도 일단 최대 8개
    for (auto& healFont : _healFonts)
    {
        healFont = UI_HealFont::Create(levelIndex);
        CHECK_NULL(healFont, E_FAIL);
    }

    // Ap 폰트
    for (auto& addAP : _addAPs)
    {
        addAP = UI_AddAP::Create(levelIndex);
        CHECK_NULL(addAP, E_FAIL);
    }

    Bind_BattleDelegates();

    if (FAILED(Ensure_BattleWorldItemCards()))
        LOG_WARN("BattleWorldUI item cards editor target preload failed.");

    return S_OK;
}

void GameplayUI_Battle::Update(float timeDelta)
{
    Update_BattleCommandHUD(timeDelta); // 플레이어 전투 입력 허드
    Update_BattleSystemHUD(timeDelta);  // 파티 portrait HP/AP HUD
    Update_BattleSystemIntroState(timeDelta);
    Update_MonsterHpBar(timeDelta);     // 몬스터 체력
    Update_BossHpBar(timeDelta);
    Update_PlayerHpBar(timeDelta);
    Update_CrossHair(timeDelta);        // Free Aim 크로스헤어
    Update_BattleDefense(timeDelta);    // 몬스터 턴 방어 (좌측 하단)
    Update_CancelAim(timeDelta);        // Free Aim 취소
    Update_ChoiceTarget(timeDelta);     // 타겟 선택 UI (F 대상 선택, ESC 뒤로)
    Update_EscapeSkipTurn(timeDelta);   // ChooseAction 중 C 도망 / TAB 턴 넘기기
    Update_AttackTooltip(timeDelta);    // AttackTargeting 중 기본 공격 설명 툴팁
    Update_BattleMechanism(timeDelta);  // 플레이어 공격 액션 중 캐릭터별 메커니즘 UI
    Update_TurnOrder(timeDelta);        // 좌측상단 턴 UI
    Update_DamageFonts(timeDelta);      // 데미지 폰트
    Update_DestroyShields(timeDelta);   // 쉴드 파괴 아이콘
    Update_HealFonts(timeDelta);        // 힐 폰트
    Update_ReactionResult(timeDelta);   // 회피/패링 성공 텍스트 수명 갱신
    Update_BattleVictory(timeDelta);    // 전투 종료 승리 UI
    Update_BattleItemPanel(timeDelta);  // 아이템
    Update_BattleSkillPanel(timeDelta); // 스킬
    Update_BattleAnnounce(timeDelta);   // 스킬
    Update_SuddenAttack(timeDelta);     // 선제 공격
    Update_AddAPs(timeDelta);           // AP 증가
    Update_QTE(GAME->Get_UnscaledTimeDelta());
    Update_MarkEffect(timeDelta);
    Update_BattleTutorial(GAME->Get_UnscaledTimeDelta());

    if (_battleTransition)
        _battleTransition->Update(timeDelta);
}

void GameplayUI_Battle::Update_Input()
{
    if (_battleTutorial != nullptr && _battleTutorial->Is_ShowingTutorial())
    {
        if (_battleController)
        {
            if (auto tutorialController = _battleController->Get_BattleTutorialController())
            {
                // 회피/카운터는 Q/E 입력이 실제 전투 처리까지 이어지고, 나머지는 F로 닫는다.
                if (tutorialController->Can_CloseByConfirmInput() && GAME->KeyDown(KEY_TYPE::F))
                {
                    tutorialController->Close_Tutorial();
                }
            }
        }

        return;
    }
}

vector<RuntimeUIEntry> GameplayUI_Battle::Get_RuntimeUIEntries() const
{
    vector<RuntimeUIEntry> entries{};
    entries.reserve(24);

    Append_RuntimePanelEntry(entries, "BattleTransition", _battleTransition.get());
    Append_RuntimePanelEntry(entries, "BattleCommandHUD", _battleCommandHUD.get());
    Append_RuntimePanelEntry(entries, "BattleSystemHUD", _battleSystemHUD.get());
    Append_RuntimePanelEntry(entries, "MonsterHpBar", _monsterHpBar.get());
    Append_RuntimePanelEntry(entries, "BossHpBar", _bossHpBar.get());
    Append_RuntimePanelEntry(entries, "PlayerHpBar", _playerHpBar.get());
    Append_RuntimePanelEntry(entries, "CrossHair", _crossHair.get());
    Append_RuntimePanelEntry(entries, "BattleDefense", _battleDefense.get());
    Append_RuntimePanelEntry(entries, "CancelAim", _cancelAim.get());
    Append_RuntimePanelEntry(entries, "ChoiceTarget", _choiceTarget.get());
    Append_RuntimePanelEntry(entries, "EscapeSkipTurn", _escapeSkipTurn.get());
    Append_RuntimePanelEntry(entries, "AttackTooltip", _attackTooltip.get());
    Append_RuntimePanelEntry(entries, "ReactionResult", _reactionResult.get());
    Append_RuntimePanelEntry(entries, "BattleVictory", _battleVictory.get());
    Append_RuntimePanelEntry(entries, "TurnOrder", _turnOrder.get());
    Append_RuntimePanelEntry(entries, "BattleMechanism", _battleMechanism.get());
    Append_RuntimePanelEntry(entries, "BattleItemPanel", _battleItemPanel.get());
    Append_RuntimeWorldPlacementEntry(entries, "BattleWorldItemCards", _battleWorldItemCards.get());
    Append_RuntimePanelEntry(entries, "BattleSkillPanel", _battleSkillPanel.get());
    Append_RuntimePanelEntry(entries, "BattleAnnounce", _battleAnnounce.get());
    Append_RuntimePanelEntry(entries, "SuddenAttack", _suddenAttack.get());
    Append_RuntimePanelEntry(entries, "QTE", _qte.get());
    Append_RuntimePanelEntry(entries, "MarkEffect", _markEffect.get());
    Append_RuntimePanelEntry(entries, "BattleTutorial", _battleTutorial.get());

    for (uint32 i = 0; i < _damageFonts.size(); ++i)
        Append_OpenedRuntimePanelEntry(entries, ("DamageFont_" + to_string(i)).c_str(), _damageFonts[i].get());

    for (uint32 i = 0; i < _destroyShields.size(); ++i)
        Append_OpenedRuntimePanelEntry(entries, ("DestroyShield_" + to_string(i)).c_str(), _destroyShields[i].get());

    for (uint32 i = 0; i < _healFonts.size(); ++i)
        Append_OpenedRuntimePanelEntry(entries, ("HealFont_" + to_string(i)).c_str(), _healFonts[i].get());

    for (uint32 i = 0; i < _addAPs.size(); ++i)
        Append_OpenedRuntimePanelEntry(entries, ("AddAP_" + to_string(i)).c_str(), _addAPs[i].get());

    return entries;
}

void GameplayUI_Battle::Play_BattleEnterTransition()
{
    CLIENT->Get_ScreenFxManager()->Transition_To_BattleField();
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

void GameplayUI_Battle::Handle_EditorStopRuntime()
{
    Reset_BattleUiRuntimeState(true);
}

bool GameplayUI_Battle::Needs_RuntimeUpdate() const
{
    if (CLIENT->Is_BattleEntering())
        return true;

    if (CLIENT->Is_BattleActive())
        return true;

    if (_isVictoryUiActive)
        return true;

    if (Is_BattleEnterTransitionPlaying())
        return true;

    return false;
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

    _healAppliedHandle =
        DamageSystem::OnHealApplied.Add(this, &GameplayUI_Battle::Handle_HealApplied);

    _battleApChangedHandle =
        _battleController->OnBattleApChanged.Add(this, &GameplayUI_Battle::Handle_BattleApChanged);

    BattleReactionController& reactionController = _battleController->Get_ReactionController();

    _reactionSucceededHandle =
        reactionController.OnReactionSucceeded.Add(this, &GameplayUI_Battle::Handle_ReactionSucceeded);

    _counterAnnounceRequestedHandle =
        reactionController.OnCounterStarted.Add(this, &GameplayUI_Battle::Handle_CounterAnnounceRequested);

    if (Shared<BattleQTEController> qteController = _battleController->Get_QTEController())
    {
        _qteStartedHandle =
            qteController->OnQTEStarted.Add(this, &GameplayUI_Battle::Handle_QTEStarted);

        _qteResolvedHandle =
            qteController->OnQTEResolved.Add(this, &GameplayUI_Battle::Handle_QTEResolved);
    }

    if (auto buffSystem = _battleController->Get_BuffSystem())
    {
        _buffAppliedHandle =
            buffSystem->OnBuffApplied.Add(this, &GameplayUI_Battle::Handle_BuffApplied);

        _buffConsumedHandle =
            buffSystem->OnBuffConsumed.Add(this, &GameplayUI_Battle::Handle_BuffConsumed);
    }

    _turnStartStatusAnnounceRequestedHandle =
        _battleController->OnTurnStartStatusAnnounceRequested.Add(
            this,
            &GameplayUI_Battle::Handle_TurnStartStatusAnnounceRequested);

    _battleWeakPointConsumedHandle =
        Mon_WeakPoint::OnBattleWeakPointConsumed.Add(this, &GameplayUI_Battle::Handle_BattleWeakPointConsumed);

    if (Shared<BattleTutorialController> tutorialController = _battleController->Get_BattleTutorialController())
    {
        _battleTutorialRequestedHandle =
            tutorialController->OnTutorialRequested.Add(
                this, &GameplayUI_Battle::Handle_BattleTutorialRequested);

        _battleTutorialClosedHandle =
            tutorialController->OnTutorialClosed.Add(
                this, &GameplayUI_Battle::Handle_BattleTutorialClosed);
    }

    if (_battleAnnounce)
    {
        _battleAnnounceFinishedHandle =
            _battleAnnounce->OnAnnounceFinished.Add(
                this,
                &GameplayUI_Battle::Handle_BattleAnnounceFinished);
    }
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

    if (_healAppliedHandle.IsValid())
        DamageSystem::OnHealApplied.Remove(_healAppliedHandle);

    BattleReactionController& reactionController = _battleController->Get_ReactionController();

    if (_reactionSucceededHandle.IsValid())
        reactionController.OnReactionSucceeded.Remove(_reactionSucceededHandle);

    if (_battleApChangedHandle.IsValid())
        _battleController->OnBattleApChanged.Remove(_battleApChangedHandle);

    if (_counterAnnounceRequestedHandle.IsValid())
        reactionController.OnCounterStarted.Remove(_counterAnnounceRequestedHandle);

    if (Shared<BattleQTEController> qteController = _battleController->Get_QTEController())
    {
        if (_qteStartedHandle.IsValid())
            qteController->OnQTEStarted.Remove(_qteStartedHandle);

        if (_qteResolvedHandle.IsValid())
            qteController->OnQTEResolved.Remove(_qteResolvedHandle);
    }

    if (auto buffSystem = _battleController->Get_BuffSystem())
    {
        if (_buffAppliedHandle.IsValid())
            buffSystem->OnBuffApplied.Remove(_buffAppliedHandle);

        if (_buffConsumedHandle.IsValid())
            buffSystem->OnBuffConsumed.Remove(_buffConsumedHandle);
    }

    if (_turnStartStatusAnnounceRequestedHandle.IsValid())
        _battleController->OnTurnStartStatusAnnounceRequested.Remove(_turnStartStatusAnnounceRequestedHandle);

    if (_battleWeakPointConsumedHandle.IsValid())
        Mon_WeakPoint::OnBattleWeakPointConsumed.Remove(_battleWeakPointConsumedHandle);

    if (Shared<BattleTutorialController> tutorialController = _battleController->Get_BattleTutorialController())
    {
        if (_battleTutorialRequestedHandle.IsValid())
            tutorialController->OnTutorialRequested.Remove(_battleTutorialRequestedHandle);

        if (_battleTutorialClosedHandle.IsValid())
            tutorialController->OnTutorialClosed.Remove(_battleTutorialClosedHandle);
    }

    if (_battleAnnounce && _battleAnnounceFinishedHandle.IsValid())
        _battleAnnounce->OnAnnounceFinished.Remove(_battleAnnounceFinishedHandle);

    _battleStartedHandle.Reset();
    _battleEndedHandle.Reset();
    _turnStartedHandle.Reset();
    _turnEndedHandle.Reset();
    _targetPreviewChangedHandle.Reset();
    _actionStartedHandle.Reset();
    _damageAppliedHandle.Reset();
    _healAppliedHandle.Reset();
    _battleApChangedHandle.Reset();
    _reactionSucceededHandle.Reset();
    _counterAnnounceRequestedHandle.Reset();

    _qteStartedHandle.Reset();
    _qteResolvedHandle.Reset();

    _buffAppliedHandle.Reset();
    _buffConsumedHandle.Reset();
    _turnStartStatusAnnounceRequestedHandle.Reset();
    _battleWeakPointConsumedHandle.Reset();

    _battleTutorialRequestedHandle.Reset();
    _battleTutorialClosedHandle.Reset();

    _battleAnnounceFinishedHandle.Reset();

    _battleController.reset();
}

void GameplayUI_Battle::Handle_BattleStarted(uint32 fieldId, bool isPlayerAdvantage)
{
    _isBattleUiActive = true;
    _isPlayerTurnUiActive = false;
    _hasTargetPreviewSnapshot = false;
    _targetPreviewSnapshot = {};
    _hasCurrentPlayerTurnActor = false;
    _currentPlayerTurnActor = {};
    _isMechanismActionActive = false;
    _activePlayerId = HeroId::END;
    _battleStartVictoryStats.clear();
    _battleStartVictoryStats = Build_BattleVictoryPlayerStats();

    GAME->Get_ScreenFxManager()->Clear_ScreenEffect();

    // 플레이어 선제공격인지
    _isPlayerAdvantageStart = isPlayerAdvantage;
    _hasPlayedSuddenAttack = false;
    _isBattleSystemIntroPlaying = true;
    _battleSystemIntroElapsed = 0.f;
    _hasPendingBattleSystemIntroActivation = false;
    _pendingBattleSystemIntroActivationHeroId = HeroId::END;

    if (_turnOrder)
        _turnOrder->Set_AnimationPlaybackEnabled(false); // 전투 진입 직후 preview는 그리되, 애니메이션은 아직 재생하지 않는다.

    UI_RuntimePanelBase* panelsToClose[] =
    {
        // 턴 시작 시 닫아야할 UI들이 있다면 여기에 추가
        _battleCommandHUD.get(),
        _battleSystemHUD.get(),
        _monsterHpBar.get(),
        _playerHpBar.get(),
        _crossHair.get(),
        _battleDefense.get(),
        _cancelAim.get(),
        _choiceTarget.get(),
        _reactionResult.get(),
        _battleMechanism.get(),
        _battleAnnounce.get(),
        _suddenAttack.get(),
        _qte.get(),
        _markEffect.get(),
        _battleTutorial.get(),
    };

    for (UI_RuntimePanelBase* panel : panelsToClose)
    {
        if (panel)
            panel->Close();
    }

    if (_markEffect)
        _markEffect->Clear_Target();

    // 이전 전투에서 남은 배틀 슬롯 정리
    for (auto& damageFont : _damageFonts)
    {
        if (damageFont)
            damageFont->Close();
    }

    for (auto& destroyShield : _destroyShields)
    {
        if (destroyShield)
            destroyShield->Close();
    }

    for (auto& addAP : _addAPs)
    {
        if (addAP)
            addAP->Close();
    }

    if (_battleSystemHUD)
    {
        vector<HeroId> battleHeroIds{};

        if (_battleController)
        {
            for (const BattleChar& hero : _battleController->Get_BattleHeroes())
                battleHeroIds.push_back(hero.heroId);
        }

        _battleSystemHUD->Set_BattleHeroIds(battleHeroIds);
        _battleSystemHUD->Open();
        _battleSystemHUD->Play_BattleSystemIntro();
    }

    Play_BattleEnterTransition(); // 전투 진입 연출은 battle UI 쪽에서 시작한다.
}

void GameplayUI_Battle::Handle_BattleEnded(EBattleEndResult result)
{
    Reset_BattleUiRuntimeState(result != EBattleEndResult::Victory);

    if (result == EBattleEndResult::Victory)
    {
        _isVictoryUiActive = true; // 실제 전투 승리 때만 결과창 갱신/표시를 허용
        _isVictoryConfirmPending = false;

        if (_battleVictory)
        {
            UI_BattleVictory::VictorySummaryDesc summary{};

            if (_battleController)
            {
                summary.rewardExp = _battleController->Get_LastVictoryRewardExp();

                if (const Shared<BattleResultRecoder> resultRecoder = _battleController->Get_BattleResultTracker())
                {
                    const BattleResultRecoder::VictoryStats& stats = resultRecoder->Get_VictoryStats();
                    summary.elapsedSec = stats.elapsedSec;
                    summary.maxDamage = stats.maxDamage;
                    summary.totalDamageDealt = stats.totalDamageDealt;
                    summary.totalDamageTaken = stats.totalDamageTaken;
                    summary.parrySuccessCount = stats.parrySuccessCount;
                    summary.dodgeSuccessCount = stats.dodgeSuccessCount;
                }

                if (const Shared<BattleSpawnRegistry> spawnRegistry = _battleController->Get_BattleSpawnRegistry())
                {
                    summary.rewardChroma = spawnRegistry->Get_BattleRewardChroma();
                    summary.defeatedMonsterPortraitGuids = spawnRegistry->Get_BattleMonsterPortraitGuids();
                }
            }

            _battleVictory->Set_VictorySummary(summary);
            _battleVictory->Set_PlayerStatus(Build_BattleVictoryPlayerStats());
            GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_StateEnd);
            _battleVictory->Open();
        }

        return;
    }
}

void GameplayUI_Battle::Handle_TurnStarted(const BattleChar& actor, BattleTurnState state)
{
    _isMechanismActionActive = false;
    _activePlayerId = HeroId::END;

    if (_turnOrder)
        _turnOrder->Notify_TurnStarted(); // 실제 턴이 시작된 뒤 다음 preview 갱신에서 turn order 애니메이션을 허용한다.

    // 턴 시작 시, 메커니즘 닫아주고
    if (_battleMechanism)
        _battleMechanism->Close();

    // 플레이어 턴인지
    _isPlayerTurnUiActive =
        (actor.statCom != nullptr && actor.statCom->Is_Player());

    if (_isPlayerTurnUiActive)
    {
        _currentPlayerTurnActor = actor;
        _hasCurrentPlayerTurnActor = true;
        _activePlayerId = actor.heroId;

        if (_battleMechanism)
            _battleMechanism->Show_Hero(_activePlayerId);

        if (_battleSystemHUD)
        {
            if (_isBattleSystemIntroPlaying)
            {
                _pendingBattleSystemIntroActivationHeroId = actor.heroId;
                _hasPendingBattleSystemIntroActivation = true;
            }
            else
            {
                _battleSystemHUD->Play_PortraitActivation(actor.heroId);
            }
        }
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

    if (_suddenAttack &&
        _isPlayerAdvantageStart &&
        !_hasPlayedSuddenAttack &&
        actor.statCom != nullptr &&
        actor.statCom->Is_Player())
    {
        _suddenAttack->Play_SuddenAttack();
        _hasPlayedSuddenAttack = true; // 첫 플레이어 턴에서 1회만 재생
    }

}

void GameplayUI_Battle::Handle_TurnEnded(const BattleChar& actor)
{
    // 턴 종료 애니메이션 먼저
    if (_battleSystemHUD &&
        actor.statCom != nullptr &&
        actor.statCom->Is_Player())
    {
        _battleSystemHUD->Play_PortraitDeactivation(actor.heroId);
    }

    _isPlayerTurnUiActive = false;
    _hasCurrentPlayerTurnActor = false;
    _currentPlayerTurnActor = {};
    _isMechanismActionActive = false;
    _activePlayerId = HeroId::END;

    UI_RuntimePanelBase* panelsToClose[] =
    {
        _battleCommandHUD.get(),
        _monsterHpBar.get(),
        _crossHair.get(),
        _battleDefense.get(),
        _cancelAim.get(),
        _choiceTarget.get(),
        _reactionResult.get(),
        _battleMechanism.get(),
        _battleAnnounce.get(),
        _suddenAttack.get(),
        _qte.get(),
        // MarkingShot 징표는 다음 플레이어 공격이 시작될 때까지 유지한다.
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

        return;
    }

    _targetPreviewSnapshot = *target;
    _hasTargetPreviewSnapshot = true;
}

void GameplayUI_Battle::Handle_ActionStarted(const BattleAction& action)
{
    if (_choiceTarget)
        _choiceTarget->Close();

    // 공격 시작하면, 몬스터 Hp bar 안보이게 하네
    if (_monsterHpBar)
        _monsterHpBar->Clear_TrackedTarget();

    const BattleChar& actor = action.Get_CurChar();

    const bool isPlayerAction =
        actor.statCom != nullptr &&
        actor.statCom->Is_Player();

    const bool isAttackAction =
        action.GetType() == EBattleContext::BaseAttack ||
        action.GetType() == EBattleContext::Skill;

    _isMechanismActionActive = isPlayerAction && isAttackAction;
    _activePlayerId = _isMechanismActionActive ? actor.heroId : HeroId::END;

    if (!_isMechanismActionActive && _battleMechanism)
        _battleMechanism->Close();

    if (_battleAnnounce && isAttackAction)
        _battleAnnounce->Show_TurnStart(action.Get_CurChar(), BattleTurnState::AttackPlaying, &action.GetSkillData());

}

void GameplayUI_Battle::Handle_DamageApplied(const DamageAppliedInfo& info)
{
    if (!_isBattleUiActive)
        return;

    const BattleChar& target = info.target;
    const float damage = info.damage;

    if (info.isShieldBlocked && info.prevShield > info.currentShield)
    {
        if (target.statCom != nullptr && !target.statCom->Is_Player())
        {
            const Shared<MonsterStatCom> monsterStat =
                dynamic_pointer_cast<MonsterStatCom>(target.statCom);

            if (monsterStat && monsterStat->Is_Boss())
            {
                if (Ensure_BossHpBar())
                    _bossHpBar->Show_DamagedBoss(target, 0.f);
            }
        }

        UI_DestroyShield* selectedSlot = nullptr;
        UI_DestroyShield* oldestSlot = nullptr;
        float longestElapsed = -1.f;
        uint32 stackIndex = 0;

        for (auto& destroyShield : _destroyShields)
        {
            if (!destroyShield)
                continue;

            if (destroyShield->Is_Playing())
            {
                ++stackIndex;

                if (destroyShield->Get_Elapsed() > longestElapsed)
                {
                    longestElapsed = destroyShield->Get_Elapsed();
                    oldestSlot = destroyShield.get();
                }

                continue;
            }

            if (selectedSlot == nullptr)
                selectedSlot = destroyShield.get();
        }

        if (selectedSlot == nullptr)
            selectedSlot = oldestSlot;

        if (selectedSlot)
        {
            stackIndex = min(stackIndex, kDestroyShieldPoolSize - 1);
            selectedSlot->Play_DestroyShield(target, stackIndex);
        }

        return;
    }

    if (damage <= 0.f)
        return;

    if (target.statCom != nullptr)
    {
        if (target.statCom->Is_Player())
        {
            if (_playerHpBar)
                _playerHpBar->Show_DamagedTarget(target, damage, 1.5f);

            if (_battleSystemHUD)
                _battleSystemHUD->Play_PortraitDamaged(target.heroId);
        }
        else
        {
            const Shared<MonsterStatCom> monsterStat =
                dynamic_pointer_cast<MonsterStatCom>(target.statCom);

            if (monsterStat && monsterStat->Is_Boss())
            {
                if (Ensure_BossHpBar())
                    _bossHpBar->Show_DamagedBoss(target, damage);
            }
            else
            {
                if (_monsterHpBar)
                    _monsterHpBar->Show_DamagedTarget(target, damage, 1.5f);
            }
        }
    }

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
            ++stackIndex;

            if (damageFont->Get_Elapsed() > longestElapsed)
            {
                longestElapsed = damageFont->Get_Elapsed();
                oldestSlot = damageFont.get();
            }

            continue;
        }

        if (selectedSlot == nullptr)
            selectedSlot = damageFont.get();
    }

    if (selectedSlot == nullptr)
        selectedSlot = oldestSlot;

    if (selectedSlot == nullptr)
        return;

    stackIndex = min(stackIndex, kDamageFontPoolSize - 1);
    selectedSlot->Play_DamageFont(target, damage, stackIndex, info.isCritical);
}

void GameplayUI_Battle::Handle_HealApplied(const BattleChar& target, float healAmount)
{
    if (!_isBattleUiActive)
        return;

    if (healAmount <= 0.f)
        return;

    UI_HealFont* selectedSlot = nullptr;
    UI_HealFont* oldestSlot = nullptr;
    float longestElapsed = -1.f;
    uint32 stackIndex = 0;

    for (auto& healFont : _healFonts)
    {
        if (!healFont)
            continue;

        if (healFont->Is_Playing())
        {
            ++stackIndex;

            if (healFont->Get_Elapsed() > longestElapsed)
            {
                longestElapsed = healFont->Get_Elapsed();
                oldestSlot = healFont.get();
            }

            continue;
        }

        if (selectedSlot == nullptr)
            selectedSlot = healFont.get();
    }

    if (selectedSlot == nullptr)
        selectedSlot = oldestSlot;

    if (selectedSlot == nullptr)
        return;

    stackIndex = min(stackIndex, kHealFontPoolSize - 1);
    selectedSlot->Play_HealFont(target, healAmount, stackIndex);
}

void GameplayUI_Battle::Handle_ReactionSucceeded(BattleReactionType reactionType, const vector<BattleChar>& target)
{
    if (!_isBattleUiActive || _reactionResult == nullptr)
        return;

    Vec3 worldPos{};
    for (auto& targetChar : target)
    {
        if (!Try_GetReactionResultWorldPos(targetChar, worldPos))
            return;

    }


    Vec2 screenPos{};
    if (!Try_ProjectWorldToScreen(worldPos, screenPos))
        return;

    screenPos += Vec2(24.f, -24.f);
    _reactionResult->Show_Reaction(reactionType, screenPos);
}

void GameplayUI_Battle::Handle_BattleApChanged(const BattleChar& actor, uint32 amount, EAPChangeReason reason)
{
    if (!_isBattleUiActive)
        return;

    // 패링, 턴 스타트, 평타
    if (reason != EAPChangeReason::Parry && reason != EAPChangeReason::TurnStart && reason != EAPChangeReason::BaseAttack)
        return;

    if (amount == 0)
        return;

    if (actor.statCom != nullptr)
    {
        if (_battleSystemHUD)
        {
            _battleSystemHUD->Set_PartyBattleStat(
                actor.statCom->Get_PlayerType(),
                actor.statCom->Get_CurHp(),
                actor.statCom->Get_StatInfo().maxHp,
                actor.statCom->Get_CurAP(),
                actor.statCom->Get_MaxAP());
        }

        if (_battleCommandHUD &&
            _hasCurrentPlayerTurnActor &&
            _currentPlayerTurnActor.actor == actor.actor)
        {
            _battleCommandHUD->Set_CurrentPlayerAP(
                actor.statCom->Get_CurAP(),
                actor.statCom->Get_MaxAP());
        }
    }

    UI_AddAP* selectedSlot = nullptr;
    UI_AddAP* oldestSlot = nullptr;
    float longestElapsed = -1.f;
    uint32 stackIndex = 0;

    for (auto& addAP : _addAPs)
    {
        if (!addAP)
            continue;

        if (addAP->Is_Playing())
        {
            ++stackIndex;

            if (addAP->Get_Elapsed() > longestElapsed)
            {
                longestElapsed = addAP->Get_Elapsed();
                oldestSlot = addAP.get();
            }

            continue;
        }

        if (selectedSlot == nullptr)
            selectedSlot = addAP.get();
    }

    if (selectedSlot == nullptr)
        selectedSlot = oldestSlot;

    if (selectedSlot == nullptr)
        return;

    stackIndex = min(stackIndex, kAddAPPoolSize - 1);
    selectedSlot->Play_AddAP(actor, amount, stackIndex);
}

void GameplayUI_Battle::Handle_QTEStarted(const BattleQTERequest& request)
{
    if (_qte == nullptr)
        return;

    _qte->Play_QTE(request);
}

void GameplayUI_Battle::Handle_QTEResolved(BattleQTEResult result)
{
    if (_qte)
        _qte->Apply_ExternalResult(result);
}

void GameplayUI_Battle::Handle_BuffApplied(const BattleChar& target, Buffs buffId)
{
    // 일단 마크만
    if (!_isBattleUiActive || buffId != Buffs::Mark || _markEffect == nullptr)
        return;

    _markEffect->Show_Target(target);
}

void GameplayUI_Battle::Handle_BuffConsumed(const BattleChar& target, Buffs buffId, float bonusDamage)
{
    UNREFERENCED_PARAMETER(target);
    UNREFERENCED_PARAMETER(bonusDamage);

    if (!_isBattleUiActive)
        return;

    if (buffId == Buffs::Mark && _markEffect)
        _markEffect->Clear_Target();
}

void GameplayUI_Battle::Handle_CounterAnnounceRequested(const vector<BattleChar>& counterActors)
{
    if (!_isBattleUiActive || _battleAnnounce == nullptr)
        return;

    _battleAnnounce->Show_Counter(counterActors);
}

void GameplayUI_Battle::Handle_BattleWeakPointConsumed(const Shared<Monster_BattleBase>& monster)
{
    if (!_isBattleUiActive || _battleAnnounce == nullptr || monster == nullptr)
        return;

    BattleChar target{};
    target.actor = monster;
    target.statCom = monster->Get_Component<CombatStatCom>();
    target.ModelCom = monster->Get_Component<ModelCom>();

    _battleAnnounce->Show_WeakPointHit(target);
}

void GameplayUI_Battle::Handle_BattleTutorialRequested(const BattleTutorialRequest& request)
{
    if (_battleTutorial == nullptr)
        return;

    _battleTutorial->Show_Tutorial(request);
}

void GameplayUI_Battle::Handle_BattleTutorialClosed()
{
    if (_battleTutorial == nullptr)
        return;

    _battleTutorial->Close();
}

void GameplayUI_Battle::Handle_TurnStartStatusAnnounceRequested(const BattleChar& target, Buffs buffId)
{
    if (!_isBattleUiActive || _battleAnnounce == nullptr)
        return;

    if (buffId == Buffs::Burn)
    {
        _battleAnnounce->Show_BurnTurnStart(target, true);
        return;
    }

    if (buffId == Buffs::Stun)
    {
        _battleAnnounce->Show_StunTurnSkip(target, true);
        return;
    }
}

void GameplayUI_Battle::Handle_BattleAnnounceFinished()
{
    if (_battleController)
        _battleController->Complete_TurnStartStatusAnnounce();
}

void GameplayUI_Battle::Update_BattleCommandHUD(float timeDelta)
{
    Hide_BattleWorldCommandCards();

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

    if (!_hasCurrentPlayerTurnActor || _currentPlayerTurnActor.statCom == nullptr)
    {
        _battleCommandHUD->Close();
        return;
    }

    Vec3 worldPos{};
    if (!Try_GetBattleCommandHudWorldPos(_currentPlayerTurnActor, worldPos))
    {
        _battleCommandHUD->Close();
        return;
    }

    if (currentState == BattleTurnState::ChooseAction)
    {
        _battleCommandHUD->Open();
        _battleCommandHUD->Set_TargetWorldPosition(worldPos);

        // 현재 턴 영웅을 HUD에 넘겨 Hero별 오프셋 세팅이 되게
        _battleCommandHUD->Set_CurrentHeroId(_currentPlayerTurnActor.heroId);

        _battleCommandHUD->Set_CurrentPlayerAP(
            _currentPlayerTurnActor.statCom->Get_CurAP(),
            _currentPlayerTurnActor.statCom->Get_MaxAP());

        _battleCommandHUD->Refresh_ForState(currentState);

        _battleCommandHUD->Update(timeDelta);
    }
    else
    {
        _battleCommandHUD->Close();
    }

    Update_BattleWorldCommandCards(worldPos, currentState);
}

HRESULT GameplayUI_Battle::Ensure_BattleWorldCommandCards()
{
    if (_battleWorldCommandCards != nullptr)
        return S_OK;

    _battleWorldCommandCards = BattleWorldUI_CommandCards::Create(GAME->Get_Device(), GAME->Get_Context());
    CHECK_NULL(_battleWorldCommandCards, E_FAIL);

    return S_OK;
}

void GameplayUI_Battle::Hide_BattleWorldCommandCards()
{
    if (_battleWorldCommandCards)
        _battleWorldCommandCards->Hide();
}

void GameplayUI_Battle::Update_BattleWorldCommandCards(const Vec3& commandHudWorldPos, BattleTurnState currentState)
{
    if (FAILED(Ensure_BattleWorldCommandCards()))
        return;

    _battleWorldCommandCards->Show_ForBattleState(commandHudWorldPos, currentState);
    _battleWorldCommandCards->Late_Update(0.f);
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

    const vector<BattleChar>& heroes = battleController->Get_BattleHeroes();

    vector<HeroId> battleHeroIds{};
    battleHeroIds.reserve(heroes.size());

    for (const BattleChar& hero : heroes)
        battleHeroIds.push_back(hero.heroId);

    _battleSystemHUD->Set_BattleHeroIds(battleHeroIds);
    _battleSystemHUD->Open(); // 전투 중에는 파티 portrait HUD를 계속 유지한다.

    _battleSystemHUD->Clear_GustaveMechanismCharge();
    _battleSystemHUD->Clear_LuneMechanismStains(); // 이번 프레임 루네 상태를 현재 mechanism 값으로 다시 연다.
    _battleSystemHUD->Clear_MaelleMechanismStance();

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

        if (hero.heroId == HeroId::Gustave)
        {
            _battleSystemHUD->Set_GustaveMechanismCharge(hero.mechanismCom);
        }
        else if (hero.heroId == HeroId::Lune)
        {
            _battleSystemHUD->Set_LuneMechanismStains(hero.mechanismCom);
        }
        else if (hero.heroId == HeroId::Maelle)
        {
            _battleSystemHUD->Set_MaelleMechanismStance(hero.mechanismCom);
        }
    }

    _battleSystemHUD->Update(timeDelta);
}

void GameplayUI_Battle::Update_MonsterHpBar(float timeDelta)
{
    if (_monsterHpBar == nullptr)
        return;

    if (!_isBattleUiActive)
    {
        _monsterHpBar->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    const BattleChar* trackedTarget = nullptr;

    if (battleController != nullptr)
    {
        if (const BattleFreeAimHitTarget* freeAimTarget = battleController->Get_FreeAimHitTarget())
            trackedTarget = &freeAimTarget->target; // HP bar는 약점 여부가 아니라 실제 몬스터 BattleChar만 추적하면 된다.

        if (trackedTarget == nullptr)
            trackedTarget = battleController->Get_TargetPreview();
    }

    if (trackedTarget != nullptr)
        _monsterHpBar->Show_TrackedTarget(*trackedTarget);
    else
        _monsterHpBar->Clear_TrackedTarget();

    _monsterHpBar->Update(timeDelta);
}

bool GameplayUI_Battle::Ensure_BossHpBar()
{
    if (_bossHpBar)
        return true;

    _bossHpBar = UI_BossHpBar::Create(_levelIndex);
    return _bossHpBar != nullptr;
}

void GameplayUI_Battle::Update_BossHpBar(float timeDelta)
{
    if (_bossHpBar == nullptr)
    {
        const auto battleController = CLIENT->Get_BattleController();
        if (battleController == nullptr || !battleController->Is_BattleStarted())
            return;
    }

    if (!_isBattleUiActive)
    {
        if (_bossHpBar)
            _bossHpBar->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        if (_bossHpBar)
            _bossHpBar->Close();
        return;
    }

    const vector<BattleChar>& enemies = battleController->Get_BattleEnemies();

    for (const BattleChar& enemy : enemies)
    {
        const Shared<MonsterStatCom> monsterStat =
            enemy.statCom ? dynamic_pointer_cast<MonsterStatCom>(enemy.statCom) : nullptr;

        if (monsterStat == nullptr || !monsterStat->Is_Boss())
            continue;

        if (enemy.statCom->Get_AliveStatus() != EAliveStatus::Alive)
            continue;

        if (!Ensure_BossHpBar())
            return;

        _bossHpBar->Show_Boss(enemy);
        _bossHpBar->Update(timeDelta);
        return;
    }

    if (_bossHpBar)
        _bossHpBar->Clear_Boss();
}

void GameplayUI_Battle::Update_PlayerHpBar(float timeDelta)
{
    if (_playerHpBar == nullptr)
        return;

    if (!_isBattleUiActive)
    {
        _playerHpBar->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    const BattleChar* trackedTarget = nullptr;
    const BattleTurnState currentState =
        battleController ? battleController->Get_CurrentTurnState() : BattleTurnState::None;

    if (currentState == BattleTurnState::ItemTargeting)
    {
        trackedTarget = battleController->Get_TargetPreview();
    }

    if (trackedTarget != nullptr &&
        trackedTarget->statCom != nullptr &&
        trackedTarget->statCom->Is_Player())
    {
        _playerHpBar->Show_TrackedTarget(*trackedTarget);
    }
    else
    {
        _playerHpBar->Clear_TrackedTarget();
    }

    _playerHpBar->Update(timeDelta);
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
    _crossHair->Set_HitState(battleController->Has_FreeAimHitTarget());

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

    const BattleChar& currentActor = battleController->Get_TurnAction().Get_CurChar();
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

    const BattleTurnState turnState = battleController->Get_CurrentTurnState();

    if (turnState == BattleTurnState::AttackTargeting ||
        turnState == BattleTurnState::SkillTargeting)
    {
        _choiceTarget->Open();
        _choiceTarget->Update(timeDelta);
        return;
    }

    if (turnState != BattleTurnState::ItemTargeting)
    {
        _choiceTarget->Close();
        return;
    }

    const BattleAction& turnAction = battleController->Get_TurnAction();
    const BattleChar& actor = turnAction.Get_CurChar();
    if (actor.ModelCom != nullptr &&
        actor.ModelCom->Get_AnimationStateIndex() == ETOI(BattleAnimType::Item_Use))
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

void GameplayUI_Battle::Update_DestroyShields(float timeDelta)
{
    for (auto& destroyShield : _destroyShields)
    {
        if (destroyShield)
            destroyShield->Update(timeDelta);
    }
}

void GameplayUI_Battle::Update_HealFonts(float timeDelta)
{
    for (auto& healFont : _healFonts)
    {
        if (healFont)
            healFont->Update(timeDelta);
    }
}

void GameplayUI_Battle::Update_AddAPs(float timeDelta)
{
    for (auto& addAP : _addAPs)
    {
        if (addAP)
            addAP->Update(timeDelta);
    }
}

void GameplayUI_Battle::Update_EscapeSkipTurn(float timeDelta)
{
    if (_escapeSkipTurn == nullptr)
        return;

    if (!_isBattleUiActive || !_isPlayerTurnUiActive)
    {
        if (!_isBattleUiActive)
        {
            _escapeSkipTurn->Close_Immediately();
            return;
        }

        _escapeSkipTurn->Close();
        _escapeSkipTurn->Update(timeDelta);
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _escapeSkipTurn->Close_Immediately();
        return;
    }

    const BattleTurnState turnState = battleController->Get_CurrentTurnState();

    if (turnState != BattleTurnState::ChooseAction &&
        turnState != BattleTurnState::ChooseItem &&
        turnState != BattleTurnState::ChooseSkill)
    {
        _escapeSkipTurn->Close();
        _escapeSkipTurn->Update(timeDelta);
        return;
    }

    _escapeSkipTurn->Open();
    _escapeSkipTurn->Update(timeDelta);
}

void GameplayUI_Battle::Update_AttackTooltip(float timeDelta)
{
    if (_attackTooltip == nullptr)
        return;

    if (!_isBattleUiActive)
    {
        _attackTooltip->Close_Immediately();
        return;
    }

    if (!_isPlayerTurnUiActive)
    {
        _attackTooltip->Close();
        _attackTooltip->Update(timeDelta);
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _attackTooltip->Close_Immediately();
        return;
    }

    if (battleController->Get_CurrentTurnState() != BattleTurnState::AttackTargeting)
    {
        _attackTooltip->Close();
        _attackTooltip->Update(timeDelta);
        return;
    }

    // AttackTargeting 동안만 open
    _attackTooltip->Open();
    _attackTooltip->Update(timeDelta);
}

void GameplayUI_Battle::Update_ReactionResult(float timeDelta)
{
    if (_reactionResult == nullptr)
        return;

    if (!_isBattleUiActive)
    {
        _reactionResult->Close();
        return;
    }

    _reactionResult->Update(timeDelta);
}

void GameplayUI_Battle::Update_BattleVictory(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta); // 월드 timeDelta 말고, 배속 적용 전 Unscaled Time 써야함

    if (_battleVictory == nullptr)
        return;

    const float uiTimeDelta = GAME->Get_UnscaledTimeDelta();

    if (_isVictoryUiActive)
    {
        _battleVictory->Open();
        _battleVictory->Update(uiTimeDelta);

        if (!_isVictoryConfirmPending && GAME->KeyDown(KEY_TYPE::F))
        {
            const bool requested = CLIENT->Request_ScreenFadeTransition(
                [this]()
                {
                    _isVictoryUiActive = false;
                    _isVictoryConfirmPending = false;

                    if (_battleVictory)
                        _battleVictory->Close_Immediately();

                    if (_battleController)
                        _battleController->Confirm_Victory();
                });

            if (requested)
                _isVictoryConfirmPending = true;
        }

        return;
    }

    _battleVictory->Close();
    _battleVictory->Update(uiTimeDelta);

    if (_isVictoryConfirmPending &&
        !_battleVictory->Is_Open() &&
        !_battleVictory->Is_ClosingWithOutro())
    {
        _isVictoryConfirmPending = false;

        if (_battleController)
            _battleController->Confirm_Victory();
    }
}

void GameplayUI_Battle::Update_BattleMechanism(float timeDelta)
{
    if (_battleMechanism == nullptr)
        return;

    if (!_isBattleUiActive || !_isPlayerTurnUiActive || _activePlayerId == HeroId::END)
    {
        _battleMechanism->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _battleMechanism->Close();
        return;
    }

    const BattleTurnState state = battleController->Get_CurrentTurnState();

    if (state == BattleTurnState::ChooseAction ||
        state == BattleTurnState::AttackTargeting ||
        state == BattleTurnState::ChooseSkill ||
        state == BattleTurnState::SkillTargeting)
    {
        _battleMechanism->Show_Hero(_activePlayerId);
        _battleMechanism->Sync_BattleChar(_currentPlayerTurnActor);
        _battleMechanism->Update(timeDelta);
        return;
    }

    if (state != BattleTurnState::AttackPlaying)
    {
        _battleMechanism->Close();
        return;
    }

    const BattleAction& action = battleController->Get_TurnAction();
    const vector<BattleActionPhase>& phases = action.GetSkillData().AnimPhases;

    if (action.Get_CurChar().actor == nullptr || phases.empty() || action.GetCurAnimIndex() >= phases.size())
    {
        _battleMechanism->Close();
        return;
    }

    _battleMechanism->Show_Hero(_activePlayerId);
    _battleMechanism->Sync_BattleChar(action.Get_CurChar());
    _battleMechanism->Update(timeDelta);
}

void GameplayUI_Battle::Update_BattleItemPanel(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    Hide_BattleWorldItemCards();

    if (_battleItemPanel == nullptr)
        return;

    auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_PlayerTurnActive())
    {
        _battleItemPanel->Close();
        return;
    }

    const Item_Data* healing = CLIENT->Get_Item(BattleItemType::HealingTint);
    const Item_Data* energy = CLIENT->Get_Item(BattleItemType::EnergyTint);
    const Item_Data* revive = CLIENT->Get_Item(BattleItemType::ReviveTint);

    if (healing && energy && revive)
        _battleItemPanel->Refresh_ItemTexts(*healing, *energy, *revive);

    const BattleTurnState turnState = battleController->Get_CurrentTurnState();
    const ActionRequest& turnRequest = battleController->Get_TurnActionRequest();

    switch (turnState)
    {
    case BattleTurnState::ChooseItem:
        _battleItemPanel->Close();
        Update_BattleWorldItemCards(turnRequest.actor, turnRequest.itemType, turnState);
        break;

    case BattleTurnState::ItemTargeting:
        _battleItemPanel->Open_Tooltip(turnRequest.itemType);
        break;

    default:
        _battleItemPanel->Close();
        break;
    }
}

void GameplayUI_Battle::Update_BattleSkillPanel(float timeDelta)
{
    Hide_BattleWorldSkillCards();

    if (_battleSkillPanel == nullptr)
        return;

    auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr)
    {
        _battleSkillPanel->Close_Immediately();
        return;
    }

    if (!battleController->Is_PlayerTurnActive())
    {
        _battleSkillPanel->Close();
        _battleSkillPanel->Update(timeDelta);
        return;
    }

    const BattleTurnState turnState = battleController->Get_CurrentTurnState();
    const ActionRequest& turnRequest = battleController->Get_TurnActionRequest();

    switch (turnState)
    {
    case BattleTurnState::ChooseSkill:
        if (turnRequest.actor.statCom)
        {
            _battleSkillPanel->Open_MainPanel(turnRequest.actor.statCom->Get_PlayerType());
            _battleSkillPanel->Update(timeDelta);
            Update_BattleWorldSkillCards(turnState, turnRequest.actor, turnRequest.skillKey);
        }
        else
        {
            _battleSkillPanel->Close_Immediately();
        }
        break;

    case BattleTurnState::SkillTargeting:
        _battleSkillPanel->Open_Tooltip(turnRequest.skillKey);
        _battleSkillPanel->Update(timeDelta);
        Update_BattleWorldSkillCards(turnState, turnRequest.actor, turnRequest.skillKey);
        break;

    default:
        _battleSkillPanel->Close();
        _battleSkillPanel->Update(timeDelta);
        break;
    }
}

HRESULT GameplayUI_Battle::Ensure_BattleWorldItemCards()
{
    if (_battleWorldItemCards != nullptr)
        return S_OK;

    _battleWorldItemCards = BattleWorldUI_ItemCards::Create(GAME->Get_Device(), GAME->Get_Context());
    CHECK_NULL(_battleWorldItemCards, E_FAIL);

    return S_OK;
}

void GameplayUI_Battle::Hide_BattleWorldItemCards()
{
    if (_battleWorldItemCards)
        _battleWorldItemCards->Hide();
}

void GameplayUI_Battle::Update_BattleWorldItemCards(
    const BattleChar& actor,
    BattleItemType selectedItemType,
    BattleTurnState currentState)
{
    if (actor.statCom == nullptr)
        return;

    Vec3 worldPos{};
    if (!Try_GetBattleCommandHudWorldPos(actor, worldPos))
        return;

    if (FAILED(Ensure_BattleWorldItemCards()))
        return;

    _battleWorldItemCards->Show_ForBattleState(worldPos, currentState, selectedItemType, actor.heroId);
    _battleWorldItemCards->Late_Update(0.f);
}

HRESULT GameplayUI_Battle::Ensure_BattleWorldSkillCards()
{
    if (_battleWorldSkillCards != nullptr)
        return S_OK;

    _battleWorldSkillCards = BattleWorldUI_SkillCards::Create(GAME->Get_Device(), GAME->Get_Context());
    CHECK_NULL(_battleWorldSkillCards, E_FAIL);

    return S_OK;
}

void GameplayUI_Battle::Hide_BattleWorldSkillCards()
{
    if (_battleWorldSkillCards)
        _battleWorldSkillCards->Hide();
}

void GameplayUI_Battle::Update_BattleWorldSkillCards(
    BattleTurnState currentState,
    const BattleChar& actor,
    const string& selectedSkillKey)
{
    if (actor.statCom == nullptr)
        return;

    Vec3 worldPos{};
    if (!Try_GetBattleCommandHudWorldPos(actor, worldPos))
        return;

    if (FAILED(Ensure_BattleWorldSkillCards()))
        return;

    _battleWorldSkillCards->Show_ForBattleState(worldPos, currentState, actor, selectedSkillKey);
    _battleWorldSkillCards->Late_Update(0.f);
}

void GameplayUI_Battle::Update_BattleAnnounce(float timeDelta)
{
    if (_battleAnnounce == nullptr)
        return;

    _battleAnnounce->Update(timeDelta);
}

void GameplayUI_Battle::Update_SuddenAttack(float timeDelta)
{
    if (_suddenAttack == nullptr)
        return;

    _suddenAttack->Update(timeDelta);
}

void GameplayUI_Battle::Update_QTE(float timeDelta)
{
    if (_qte == nullptr)
        return;

    if (!_isBattleUiActive || _battleController == nullptr)
    {
        _qte->Close();
        return;
    }

    if (_battleTutorial != nullptr && _battleTutorial->Is_ShowingTutorial())
    {
        if (auto tutorialController = _battleController->Get_BattleTutorialController())
        {
            if (tutorialController->Get_ActiveStep() == BattleTutorialStep::QTE)
                return;
        }
    }

    _qte->Update(timeDelta);

    BattleQTEResult result = BattleQTEResult::None;
    if (_qte->Try_ConsumeResult(result))
    {
        if (Shared<BattleQTEController> qteController = _battleController->Get_QTEController())
            qteController->Resolve(result);
    }
}

void GameplayUI_Battle::Update_MarkEffect(float timeDelta)
{
    if (_markEffect)
        _markEffect->Update(timeDelta);
}

void GameplayUI_Battle::Update_BattleSystemIntroState(float timeDelta)
{
    if (!_isBattleSystemIntroPlaying)
        return;

    _battleSystemIntroElapsed += max(0.f, timeDelta);
    if (_battleSystemIntroElapsed < kBattleSystemIntroDurationSec)
        return;

    _isBattleSystemIntroPlaying = false;
    _battleSystemIntroElapsed = 0.f;

    if (!_hasPendingBattleSystemIntroActivation)
        return;

    const HeroId heroId = _pendingBattleSystemIntroActivationHeroId;
    _hasPendingBattleSystemIntroActivation = false;
    _pendingBattleSystemIntroActivationHeroId = HeroId::END;

    if (_battleSystemHUD && heroId != HeroId::END)
        _battleSystemHUD->Play_PortraitActivation(heroId);
}

void GameplayUI_Battle::Update_BattleTutorial(float timeDelta)
{
    if (_battleTutorial == nullptr)
        return;

    _battleTutorial->Update(timeDelta);
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

bool GameplayUI_Battle::Try_GetReactionResultWorldPos(const BattleChar& target, Vec3& outWorldPos)
{
    if (target.actor == nullptr || target.actor->Get_Transform() == nullptr)
        return false;

    if (auto* battleActor = dynamic_cast<Character_BattleBase*>(target.actor.get()))
    {
        if (battleActor->Try_GetDamageFeedbackWorldPosition(outWorldPos))
        {
            outWorldPos += Vec3(0.35f, 0.25f, 0.f);
            return true;
        }
    }

    outWorldPos = target.actor->Get_Transform()->Get_WorldPosition();
    outWorldPos += Vec3(0.35f, 1.5f, 0.f);

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
        entry.actorToken = reinterpret_cast<uint64>(actor.actor.get());

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

vector<GameplayUI_Battle::VictoryStatDesc> GameplayUI_Battle::Build_BattleVictoryPlayerStats() const
{
    vector<VictoryStatDesc> victoryStats{};

    if (_battleController == nullptr)
        return victoryStats;

    const uint32 rewardExp = _battleController->Get_LastVictoryRewardExp();
    const vector<BattleChar>& battleHeroes = _battleController->Get_BattleHeroes();
    const bool useCenterSingleSlot = battleHeroes.size() == 1;

    for (const BattleChar& hero : battleHeroes)
    {
        const Shared<PlayerStatCom> playerStat =
            dynamic_pointer_cast<PlayerStatCom>(hero.statCom);

        if (playerStat == nullptr)
            continue;

        const PlayerStatCom::PRuntimeCombatStat& runtimeStat =
            playerStat->Get_PlayerRuntimeStat();

        VictoryStatDesc stat{};
        stat.playerType = playerStat->Get_PlayerType();
        stat.visualSlotIndex = useCenterSingleSlot ? 1u : UINT_MAX;
        stat.portraitGuid = playerStat->Get_StatInfo().portraitGuid;
        stat.displayName = Resolve_BattleVictoryPlayerName(stat.playerType);
        stat.level = runtimeStat.CurLevel;
        stat.curExp = runtimeStat.CurExp;
        stat.maxExp = runtimeStat.CurMaxExp;
        stat.prevLevel = stat.level;
        stat.prevExp = stat.curExp;
        stat.prevMaxExp = stat.maxExp;

        if (rewardExp > 0)
        {
            uint32 rewindLevel = stat.level;
            uint32 rewindExp = stat.curExp;
            uint32 remainingReward = rewardExp;

            while (remainingReward > 0)
            {
                if (remainingReward <= rewindExp)
                {
                    rewindExp -= remainingReward;
                    remainingReward = 0;
                    break;
                }

                remainingReward -= rewindExp;

                if (rewindLevel <= 1)
                {
                    rewindExp = 0;
                    remainingReward = 0;
                    break;
                }

                --rewindLevel;
                rewindExp = CLIENT->Get_NextExp(rewindLevel);
            }

            stat.prevLevel = rewindLevel;
            stat.prevExp = rewindExp;
            stat.prevMaxExp = CLIENT->Get_NextExp(rewindLevel);
        }
        else
        {
            for (const VictoryStatDesc& startStat : _battleStartVictoryStats)
            {
                if (startStat.playerType != stat.playerType)
                    continue;

                stat.prevLevel = startStat.level;
                stat.prevExp = startStat.curExp;
                stat.prevMaxExp = startStat.maxExp;
                break;
            }
        }

        victoryStats.push_back(stat);
    }

    return victoryStats;
}

void GameplayUI_Battle::Reset_BattleUiRuntimeState(bool closeVictoryUi)
{
    _isBattleUiActive = false;
    _isPlayerTurnUiActive = false;
    _hasTargetPreviewSnapshot = false;
    _targetPreviewSnapshot = {};
    _hasCurrentPlayerTurnActor = false;
    _currentPlayerTurnActor = {};
    _isVictoryUiActive = false;
    _isVictoryConfirmPending = false;
    _isMechanismActionActive = false;
    _activePlayerId = HeroId::END;
    _battleStartVictoryStats.clear();
    _isBattleSystemIntroPlaying = false;
    _battleSystemIntroElapsed = 0.f;
    _hasPendingBattleSystemIntroActivation = false;
    _pendingBattleSystemIntroActivationHeroId = HeroId::END;

    Hide_BattleWorldCommandCards();
    Hide_BattleWorldItemCards();
    Hide_BattleWorldSkillCards();

    if (_turnOrder)
        _turnOrder->Set_AnimationPlaybackEnabled(false);

    UI_RuntimePanelBase* panelsToClose[] =
    {
        _battleCommandHUD.get(),
        _battleSystemHUD.get(),
        _monsterHpBar.get(),
        _bossHpBar.get(),
        _playerHpBar.get(),
        _crossHair.get(),
        _battleDefense.get(),
        _cancelAim.get(),
        _choiceTarget.get(),
        _escapeSkipTurn.get(),
        _attackTooltip.get(),
        _reactionResult.get(),
        _turnOrder.get(),
        _battleMechanism.get(),
        _battleItemPanel.get(),
        _battleSkillPanel.get(),
        _battleAnnounce.get(),
        _suddenAttack.get(),
        _qte.get(),
        _markEffect.get(),
        _battleTutorial.get(),
    };

    for (auto* panel : panelsToClose)
    {
        if (panel)
            panel->Close();
    }

    if (_escapeSkipTurn)
        _escapeSkipTurn->Close_Immediately();

    if (_attackTooltip)
        _attackTooltip->Close_Immediately();

    if (_battleSkillPanel)
        _battleSkillPanel->Close_Immediately();

    if (_markEffect)
        _markEffect->Clear_Target();

    for (auto& damageFont : _damageFonts)
    {
        if (damageFont)
            damageFont->Close();
    }

    for (auto& destroyShield : _destroyShields)
    {
        if (destroyShield)
            destroyShield->Close();
    }

    for (auto& healFont : _healFonts)
    {
        if (healFont)
            healFont->Close();
    }

    for (auto& addAP : _addAPs)
    {
        if (addAP)
            addAP->Close();
    }


    Stop_BattleEnterTransition(); // 진입 중 F4에서도 페이드 잔상을 남기지 않는다.

    if (closeVictoryUi && _battleVictory)
        _battleVictory->Close_Immediately();
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

    if (_battleWorldCommandCards)
    {
        _battleWorldCommandCards->Free();
        _battleWorldCommandCards.reset();
    }

    if (_battleWorldItemCards)
    {
        _battleWorldItemCards->Free();
        _battleWorldItemCards.reset();
    }

    if (_battleWorldSkillCards)
    {
        _battleWorldSkillCards->Free();
        _battleWorldSkillCards.reset();
    }

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
    Release_UI(_bossHpBar);
    Release_UI(_playerHpBar);
    Release_UI(_crossHair);
    Release_UI(_battleDefense);
    Release_UI(_cancelAim);
    Release_UI(_escapeSkipTurn);
    Release_UI(_attackTooltip);
    Release_UI(_choiceTarget);
    Release_UI(_reactionResult);
    Release_UI(_battleMechanism);
    Release_UI(_battleItemPanel);
    Release_UI(_battleSkillPanel);
    Release_UI(_battleAnnounce);
    Release_UI(_suddenAttack);
    Release_UI(_qte);
    Release_UI(_markEffect);
    Release_UI(_battleTutorial);

    for (auto& damageFont : _damageFonts)
        Release_UI(damageFont);

    for (auto& destroyShield : _destroyShields)
        Release_UI(destroyShield);

    for (auto& healFont : _healFonts)
        Release_UI(healFont);

    for (auto& addAP : _addAPs)
        Release_UI(addAP);

    Base::Free();
}
