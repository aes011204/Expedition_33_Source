#include "BattleController.h"

#include "ANS_WeaponEcho.h"
#include "AttackPlaying.h"
#include "BuffSystem.h"
#include "Character.h"
#include "ClientInstance.h"
#include "GameDataTable.h"
#include "AttackTargeting.h"
#include "BattleStateMachine.h"
#include "ChooseAction.h"
#include "ChooseGradient.h"
#include "ChooseItem.h"
#include "ChooseSkill.h"
#include "ChooseUltimate.h"
#include "CombatStatCom.h"
#include "Create_Class.h"
#include "ItemTargeting.h"
#include "MB_Lancelier.h"
#include "Player_WorldBase.h"
#include "Player_BattleBase.h"
#include "SkillTargeting.h"
#include "EffectCom.h"
#include "Spawn_Prefab.h"
#include "UltimateTargeting.h"
#include "SkillScript.h"
#include "ModelCom.h"
#include "PartObject.h"
#include "BattleTurnCamera.h"
#include "Camera.h"
#include "TargetCamera.h"
#include "BattleActionExecutor.h"
#include "BattleAnimutils.h"
#include "BattleFollowupManager.h"
#include "BattleMechanismCom.h"
#include "Battle_Event.h"
#include "Character.h"
#include "Event_Bus.h"
#include "DamageSystem.h"
#include "FreeAimTargeting.h"
#include "PlayerStatCom.h"
#include "BattleSpawnRegistry.h"
#include "SkillTreeCom.h"
#include "BattleReactionController.h"
#include "BattleResultRecoder.h"
#include "BattleStartSpawner.h"
#include "EffectCom.h"
#include "Monster_WorldBase.h"
#include "MonsterStatCom.h"
#include "ScreenFx_Manager.h"
#include "Client_Constants.h"
#include "BattleEncounterProfile.h"
#include "BattleTutorialController.h"
#include "PlayerInventory.h"
#include "PostProcessPipeline.h"
#include "TransformCom.h"
#include "GradientSkill.h"
#include "PictosBattleSystem.h"
#include "Unlock_Manager.h"

REGISTER_GLOBAL_ENUM(BattleAnimType, "BattleAnimType")

namespace
{
    // DOF_MODIFY 
    constexpr char kBattleDOFOwnerTag[] = "BattleTurn";

    constexpr wchar_t kHealTintEffect[] = L"Effects\\Assets\\Item\\HealTint_G.effect.json";
    constexpr wchar_t kEnergyTintEffect[] = L"Effects\\Assets\\Item\\HealTint_B.effect.json";
    constexpr wchar_t kReviveTintEffect[] = L"Effects\\Assets\\Item\\HealTint_P.effect.json";
}

BattleController::BattleController()
{
}

BattleController::~BattleController()
{
    Free();
}

EAPChangeReason BattleController::FinishBattle()
{
    return EAPChangeReason::Parry;
}

HRESULT BattleController::Initialize()
{
    Set_BattleFSM();

    _battleActionExe = BattleActionExecutor::Create();
    CHECK_NULL(_battleActionExe, E_FAIL);

    _battleSpawnRegistry = BattleSpawnRegistry::Create();
    CHECK_NULL(_battleSpawnRegistry, E_FAIL);

    _qteController = BattleQTEController::Create();
    CHECK_NULL(_qteController, E_FAIL);

    _buffSystem = BuffSystem::Create();
    CHECK_NULL(_buffSystem, E_FAIL);

    _battleTutorialController = BattleTutorialController::Create();
    CHECK_NULL(_battleTutorialController, E_FAIL);

    _battleResultRecoder = BattleResultRecoder::Create();
    CHECK_NULL(_battleResultRecoder, E_FAIL);

    _pictosBattleSystem = PictosBattleSystem::Create();
    CHECK_NULL(_pictosBattleSystem, E_FAIL);

    //    _targetPreviewChangedHandle = OnTargetPreviewChanged.Add(this,&BattleController::Handle_TargetPreviewChanged);

    return S_OK;
}

void BattleController::Update(float timeDelta)
{
    if (!Should_UpdateRuntime())
        return;

    if (Update_BattleTutorialGate(timeDelta))
        return;

    if (_isBattleEntering && CLIENT->Is_SoulLikeBattleActive())
    {
        Cancel_BattleEnterPending();
        return;
    }

    if (_isBattleEntering)
    {
        if (!_hasRequestedBattleEnterCamera)
        {
            const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();

            CLIENT->Request_TargetCameraMode(TargetCameraMode::BATTLE_ENTER);
            _hasRequestedBattleEnterCamera = true;
        }

        // 슬로우 모션 지속시간은 월드 배속 안받은 Unscaled Time 가져오기
        _battleEnterSlow.elapsed += GAME->Get_UnscaledTimeDelta();

        if (_battleEnterSlow.elapsed >= _battleEnterSlow.duration)
            Complete_BattleField();

        return;
    }

    if (_isWaitingBattleTransition && _firstAnimflag == true)
    {
        // 전환이 끝난 뒤에 첫 턴 시작되게
        if (!CLIENT->Is_BattleEnterTransitionPlaying())
        {
            _isWaitingBattleTransition = false;

            StartTurn();
        }

        return;
    }

    // 배틀이 완전히 끝남 (배틀턴 나감)
    if (_battleEnded == true)
        return;

    if (_isWaitingTurnStartStatusHit)
    {
        const uint32 currentAnimState =
            _currentTurnActor.ModelCom ? _currentTurnActor.ModelCom->Get_AnimationStateIndex() : UINT32_MAX;

        const bool isStatusHitAnim =
            currentAnimState == ETOI(BattleAnimType::Hit_Light) ||
            currentAnimState == ETOI(BattleAnimType::Hit_Stun);

        const bool hitAnimFinished =
            _currentTurnActor.ModelCom == nullptr ||
            _currentTurnActor.ModelCom->Is_AnimationFinished() ||
            !isStatusHitAnim; // Hit notify가 Idle/Stun idle로 넘긴 뒤에는 턴 시작을 이어간다.

        if (!hitAnimFinished)
            return;

        _isWaitingTurnStartStatusHit = false;

        if (Try_BeginTurnStartStatusAnnounce())
            return;

        _processedTurnStartStatuses.clear();
        Continue_StartTurnAfterStatusEffects();

        return;
    }

    if (_pendingTurnStartStatusBuff != Buffs::None)
        return; // 상태 이상 아나운스가 끝나기 전에는 턴 입력/AI를 시작하지 않는다.

    _reactionController.UpdateAnimation(timeDelta);
    Update_BattleSpawnRotationReturns(timeDelta);

    if (_isPendingVictorySlow)
    {
        _victorySlow.elapsed += GAME->Get_UnscaledTimeDelta();

        if (_victorySlow.elapsed >= _victorySlow.duration)
        {
            _isPendingVictorySlow = false;
            _victorySlow.elapsed = 0.f;

            OnExitBattleField(EBattleEndResult::Victory);
            _battleEnded = true;
        }

        return; // 승리 확정 후에는 남은 액션 phase(Dash_Out/Return)를 더 진행하지 않는다.
    }

    if (!_isPendingVictorySlow && _requestEndTurnOnCurrentActorDeath)
    {
        // CounterReturn 연출 중에는 사망으로 인한 강제 턴 종료를 잠깐 미룬다.
        if (_reactionController.IsBusy())
            return;

        _requestEndTurnOnCurrentActorDeath = false;
        EndTurn();
        return;
    }

    if (!_isPendingVictorySlow)
    {
        _reactionController.Update_EchoReactionWindows(timeDelta);

        // 패링, 회피 키인풋
        const auto reactionResult = _reactionController.UpdateInput(_turnAction, _autoParryEnabled);

        Apply_ReactionApReward(reactionResult);

        // 한턴 끝나면 여기로
        if (_pendingNextTurn)
        {
            if (_battleActionExe)
                _battleActionExe->Reset();

            StartTurn();
            _pendingNextTurn = false;
            return;
        }
    }

    if (_heroes.empty() == true || _enemies.empty() == true || _currentTurnActor.actor == nullptr)
        return;

    // 적턴 인지 플레이어 턴인지에 따라
    if (!_isPendingVictorySlow &&
        _currentTurnActor.statCom && _currentTurnActor.statCom->Is_Player())
    {
        _battleFSM->Update_StateMachine(timeDelta);
    }

    // Script 다단히트 적용
    _hitSequenceRunner.Update(timeDelta);
    Update_DelayedEchoAttackSteps(timeDelta);

    if (_isFreeAimShotPlaying)
    {
        if (_freeAimShotAction.Get_CurChar().ModelCom->Is_AnimationFinished())
        {
            if (!_isPendingVictorySlow)
                Finish_FreeAimShot();

            return;
        }

        return;
    }

    if (_isActionExecuting &&
        _turnAction.Get_CurChar().actor != nullptr &&
        !_battleActionExe->IsFinished())
    {
        _battleActionExe->Update(timeDelta, _turnAction);
    }

    if (_isPendingVictorySlow)
        return;

    if (_battleResultRecoder &&
        _isBattleActive &&
        !_battleEnded &&
        !_isWaitingVictoryConfirm)
    {
        _battleResultRecoder->Update(GAME->Get_UnscaledTimeDelta());
    }

    if (_isActionExecuting &&
        _turnAction.Get_CurChar().actor != nullptr &&
        _battleActionExe->IsFinished())
    {
        if (_reactionController.IsBusy())
            return;

        const BattleChar actor = _turnAction.Get_CurChar();
        const bool isMonsterFreeTurnSkill =
            actor.actor != nullptr &&
            actor.statCom != nullptr &&
            !actor.statCom->Is_Player() &&
            _turnAction.GetType() == EBattleContext::Skill &&
            _turnAction.GetSkillData().apCost < 0;

        _isActionExecuting = false;

        if (isMonsterFreeTurnSkill)
        {
            // 몬스터 전용 free-turn 스킬은 턴을 유지한 채 다음 스킬 선택으로 다시 진입한다.
            Play_EnemyTargetedEnd();
            Run_EnemyTurn();
            return;
        }

        EndTurn();
        return;
    }
}

void BattleController::Run_EnemyTurn()
{
    const auto monster
        = dynamic_pointer_cast<Monster_BattleBase>(Get_TurnActionRequest().actor.actor);

    CHECK_NULL(monster);

    monster->Decide_Action_BTOnly(Get_TurnActionRequest(), Get_AliveHeroes());

    vector<BattleChar> targetedHeroes{};
    const Skill_Data* selectedSkill = CLIENT->Get_Skill(Get_TurnActionRequest().skillKey);

    bool targetsAllHeroes = false;
    if (selectedSkill != nullptr)
    {
        for (const TargetType targetType : selectedSkill->targetingType)
        {
            if (targetType == TargetType::AllEnemies ||
                targetType == TargetType::AllEnemiesJump)
            {
                targetsAllHeroes = true;
                break;
            }
        }
    }

    if (targetsAllHeroes)
    {
        targetedHeroes = Get_AliveHeroes();
    }
    else if (Get_TurnActionRequest().InitTarget.actor != nullptr)
    {
        targetedHeroes.push_back(Get_TurnActionRequest().InitTarget);
    }

    Play_EnemyTargetedStart(targetedHeroes);

    ExecuteActionRequest(Get_TurnActionRequest());

    // EvtBattlePresentation event;
    // event.cameraMode = BattleCameraMode::ENEMYATTACK;
    // event.subject = monster;
    // event.opponent = Get_TurnAction().target.actor;

    // GAME->Get_EventBus().Publish(event);
}

void BattleController::OnEnterBattleField(
    uint32 FieldID,
    bool isPlayerAdvantage,
    const vector<HeroId>& heroIds,
    const vector<BattleMonsterSpawnDesc>& battleMonsterSpawnDescs,
    const Shared<GameObject>& sourceObject,
    bool completeImmediately,
    int32 battleStartSpawnerId,
    const Vec3& battleStartOffset)
{
    if (_isBattleActive || _isBattleEntering)
        return;

    _isBattleEntering = true;
    _battleEnterSlow.elapsed = 0.f;
    _battleEnded = false;
    _pendingNextTurn = false;

    const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();

    // 배틀 진입 소스 오브젝트 세팅
    // ex) MW_Lancelier랑 충돌돼서 전투 시작하면 저장하고있다가, 전투 종료 후 삭제하려고
    _battleSpawnRegistry->Set_SourceWorldObject(sourceObject, GAME->Current_LevelIndex());

    // 2초 뒤 실제 배틀 생성 데이터 추가
    _pendingBattleFieldID = FieldID;
    _pendingBattleIsPlayerAdvantage = isPlayerAdvantage;
    _pendingBattleHeroIds = heroIds;
    _pendingBattleMonsterSpawnDescs = battleMonsterSpawnDescs;
    _pendingBattleStartSpawnerId = battleStartSpawnerId;
    _pendingBattleStartOffset = battleStartOffset;
    _pendingBattlePlaneYaw = 0.f;

    if (Shared<Monster_WorldBase> worldMonster = dynamic_pointer_cast<Monster_WorldBase>(sourceObject))
    {
        _pendingBattleStartSpawnerId = worldMonster->Get_BattleStartSpawnerId();
        _pendingBattleStartOffset = worldMonster->Get_BattleStartOffset();
    }

    // 월드 캐릭터 입력 막기
    GAME->Set_PlayerInputBlocked(true);
    //_worldCamera = GAME->Get_ActiveCamera();

    if (completeImmediately)
    {
        if (!GAME->Is_ActiveCameraType(ObjectType::BattleTurnCamera))
            CLIENT->Request_TargetCameraMode(TargetCameraMode::BATTLE_ENTER); // 월드/시네마틱 즉시 진입은 기존 전투 진입 카메라 요청을 유지한다.

        _hasRequestedBattleEnterCamera = true;
        Complete_BattleField(); // 시네마틱 종료 전투는 2초 슬로우 대기 없이 바로 BattleStarted까지 진행한다.
        return;
    }

    GAME->Slow_WorldTime(_battleEnterSlow.worldTimeScale, _battleEnterSlow.duration);
}

void BattleController::Cancel_BattleEnterPending()
{
    if (!_isBattleEntering || _isBattleActive)
        return;

    GAME->Stop_WorldTimeSlow();
    GAME->Set_WorldTimeScale(1.f);
    GAME->Set_PlayerInputBlocked(false);

    if (_battleSpawnRegistry)
    {
        if (Shared<Monster_WorldBase> worldMonster =
            dynamic_pointer_cast<Monster_WorldBase>(_battleSpawnRegistry->Get_SourceWorldObject()))
        {
            worldMonster->Restore_AfterBattleExit();
        }

        _battleSpawnRegistry->Clear_SourceWorldObject();
    }

    _battleEnded = false;
    _pendingNextTurn = false;
    Clear_BattleEnterPending();
}

EBattleEndResult BattleController::OnExitBattleField(EBattleEndResult result)
{
    _lastVictoryRewardExp = 0;
    _lastVictoryRewardPictosIds.clear();

    if (result == EBattleEndResult::Victory && _battleSpawnRegistry)
    {
        _lastVictoryRewardExp = _battleSpawnRegistry->Get_BattleRewardExp();
        Apply_VictoryRewardExp(_lastVictoryRewardExp); // 경험치 지급

        // 크로마 지급
        if (auto inventory = CLIENT->Get_PlayerInventory())
        {
            inventory->Add_Chroma(_battleSpawnRegistry->Get_BattleRewardChroma());
        }

        vector<string> defeatedEnemyKeys{};
        defeatedEnemyKeys.reserve(_enemies.size());

        for (const BattleChar& enemy : _enemies)
        {
            if (enemy.actor == nullptr)
                continue;

            const string& sourcePrefabName = enemy.actor->Get_SourcePrefabName();
            if (sourcePrefabName.empty())
                continue;

            defeatedEnemyKeys.emplace_back(sourcePrefabName);
        }

        vector<HeroId> victoryHeroIds{};
        victoryHeroIds.reserve(_heroes.size());

        for (const BattleChar& hero : _heroes)
        {
            if (hero.heroId == HeroId::END)
                continue;

            victoryHeroIds.emplace_back(hero.heroId);
        }

        Shared<GameObject> sourceWorldObject = _battleSpawnRegistry->Get_SourceWorldObject();

        Vec3 rewardOrigin = Vec3::Zero;
        const Vec3* rewardOriginPtr = nullptr;
        if (sourceWorldObject)
        {
            if (const Shared<TransformCom> sourceTransform = sourceWorldObject->Get_Transform())
            {
                rewardOrigin = sourceTransform->Get_WorldPosition();
                rewardOriginPtr = &rewardOrigin;
            }
        }

        const uint32 rewardLevelIndex = _battleSpawnRegistry->Get_SourceWorldObjectLevelIndex();

        if (Unlock_Manager* unlockManager = CLIENT->Get_UnlockManager())
        {
            _lastVictoryRewardPictosIds = unlockManager->Handle_BattleVictory(
                defeatedEnemyKeys,
                _activeBattleFieldID,
                rewardLevelIndex,
                rewardOriginPtr);
        }

        if (BattleFollowupManager* followupManager = CLIENT->Get_BattleFollowupManager())
        {
            followupManager->Handle_BattleVictory(
                defeatedEnemyKeys, victoryHeroIds,
                _activeBattleFieldID, sourceWorldObject);
        }
    }

    // 배틀에서 실제로 바뀐 결과를 월드 플레이어한테 전달. 체력, 경험치 등
    for (const BattleChar& hero : _heroes)
    {
        if (hero.heroId == HeroId::END)
            continue;

        const Shared<PlayerStatCom> battleStat = dynamic_pointer_cast<PlayerStatCom>(hero.statCom);
        const Shared<Player_WorldBase> worldHero = CLIENT->Find_Hero(hero.heroId);
        const Shared<PlayerStatCom> worldStat = worldHero ? worldHero->Get_Component<PlayerStatCom>() : nullptr;

        if (battleStat && worldStat)
            worldStat->Apply_BattleSyncSnapshot(battleStat->Create_BattleSyncSnapshot());
    }

    Clear_TargetPreview();
    OnBattleEnded.Broadcast(result); // battle end UI 정리는 GameplayUI_Battle에서 처리

    if (result == EBattleEndResult::Victory)
    {
        const bool isBishopCinematicBGM =
            GAME->Get_CurrentBGM() == Constants::SoundBGM::BishopBattleBossMain;

        if (Is_TutorialBattleEncounter(_activeBattleFieldID))
        {
            GAME->Stop_SoundChannel(SoundChannel::BGM);
        }
        else if (isBishopCinematicBGM)  // 주교잡고나서는 Victory 브금
        {
            GAME->Play_BGM(Constants::SoundBGM::BishopBattleBossVictory, 1.f, true, 0.f);
        }
        else
        {
            Restore_PreBattleBGM();
        }

        _isWaitingVictoryConfirm = true;
        _isBattleEntering = false;
        _isWaitingBattleTransition = false;

        GAME->Stop_WorldTimeSlow();
        GAME->Set_WorldTimeScale(0.0001f);
        GAME->Set_PlayerInputBlocked(true);

        return result;
    }

    Restore_PreBattleBGM();

    _isBattleActive = false;
    _isBattleEntering = false;
    _isWaitingBattleTransition = false;
    // DOF_MODIFY 
    End_BattleDOFOverride();

    //Restore_WorldCamera(); // 카메라 복구
    CLIENT->Restore_PreviousCamera();
    GAME->Set_PlayerInputBlocked(false);

    // 도망치기
    // 실패
    // 성공

    if (_battleSpawnRegistry)
    {
        if (result == EBattleEndResult::Neutral)
        {
            if (Shared<Monster_WorldBase> worldMonster =
                dynamic_pointer_cast<Monster_WorldBase>(_battleSpawnRegistry->Get_SourceWorldObject()))
            {
                worldMonster->Restore_AfterBattleExit(); // 도주 시 MW는 삭제하지 않고 월드 상태만 복구한다.
            }
        }

        _battleSpawnRegistry->Destroy_SpawnedObjects();  // PB/MB는 무조건 제거
        _battleSpawnRegistry->Clear_SourceWorldObject(); // Neutral은 참조만 비우고, MW 오브젝트는 남겨야한다.
    }

    Clear_BattleRuntimeState();

    _heroes.clear();
    _enemies.clear();

    _battleTurnCamera.reset(); // 도주/패배 뒤에도 이전 전투 카메라 참조를 다음 전투로 넘기지 않는다.

    Clear_BattleEnterPending();
    _activeBattleFieldID = 0;

    // 초기 애님 관련 변수 초기화
    _firstAnimflag = false;
    _firstAnimCount = 0;
    _firstAnimTargetCount = 0;

    return result;
}

void BattleController::ExecuteAction(const BattleAction& turnAction)
{
    const auto BattleChar = turnAction.Get_CurChar();
    if (!BattleChar.actor)
        return;

    OnActionStarted.Broadcast(turnAction);

    auto skillData = CLIENT->Get_Skill(turnAction.GetSkillData().skillKey);
    _reactionController.Set_CurSkillData(skillData);

    Clear_TargetPreview();

    // 일반 공격도 스킬로 들어감 구분만 BaseAttack
    switch (turnAction.GetType())
    {
    case EBattleContext::BaseAttack:
        Execute_BaseAttack();
        break;

    case EBattleContext::Skill:
        Execute_Skill();
        break;

    case EBattleContext::ConsumableItem:
        Execute_Item();
        break;
    }
}

void BattleController::ExecuteActionRequest(const ActionRequest& request)
{
    if (request.actor.actor == nullptr)
        return;

    _turnActionRequest = request;

    if (request.type == EBattleContext::FreeAimShoot)
    {
        BattleAction shotAction{};
        shotAction.Set_BattleActionFromRequest(request);
        Begin_FreeAimShot(shotAction);
        return;
    }

    _turnAction.Set_BattleActionFromRequest(request);
    ExecuteAction(_turnAction);
}

void BattleController::EndTurn()
{
    if (_qteController)
        _qteController->FailIfActive(); // 턴이 먼저 끝나면 열린 QTE는 QTEController에서 실패로 확정한다.

    _isActionExecuting = false;

    // 이번 적 턴에서 공격하는 플레이어 Targeted End 애니메이션 재생
    Play_EnemyTargetedEnd();

    if (_currentTurnActor.actor != nullptr &&
        _currentTurnActor.statCom != nullptr &&
        _currentTurnActor.statCom->Is_Player() &&
        _currentTurnActor.statCom->Get_AliveStatus() != EAliveStatus::Dead)
    {
        Request_BattleSpawnRotationReturn(_currentTurnActor, 0.2f);
    }

    // 일단 만들어 놓은 스킬 지우기
    Clear_CurrentScript();

    // 끝났다면 끝
    uint32 DeadPlayer = {};
    uint32 DeadEnemy = {};
    for (int i = 0; i < _heroes.size(); i++)
    {
        if (_heroes[i].statCom->Get_AliveStatus() == EAliveStatus::Dead)
            DeadPlayer++;
    }

    if (DeadPlayer == _heroes.size())
    {
        OnExitBattleField(EBattleEndResult::Defeat);
        _battleEnded = true;
        return;
    }

    for (int i = 0; i < _enemies.size(); i++)
    {
        if (_enemies[i].statCom->Get_AliveStatus() == EAliveStatus::Dead)
            DeadEnemy++;
    }

    if (DeadEnemy == _enemies.size())
    {
        if (_isPendingVictorySlow)
            return;

        OnExitBattleField(EBattleEndResult::Victory);
        _battleEnded = true;

        return;
    }

    Clear_TargetPreview();

    // 상태 이상 엔드
    const auto actor = _currentTurnActor;
    const float scheduleSpeed =
        _buffSystem ? _buffSystem->Resolve_TurnScheduleSpeed(actor)
        : max(actor.statCom->Get_Speed(), 1.f);

    _buffSystem->ProcessTurnEnd(_currentTurnActor);
    OnTurnEnded.Broadcast(_currentTurnActor); // 턴 종료 신호

    // 죽은애들은 큐에 다시 안들어가게
    if (actor.statCom->Get_AliveStatus() != EAliveStatus::Dead)
    {
        Push_TurnEntry(actor, _battleTime + 100.f / scheduleSpeed);
    }

    _pendingNextTurn = true;
}

void BattleController::StartTurn()
{
    _isActionExecuting = false; // 선택 대기 상태에서는 executor 완료 여부로 턴을 넘기지 않는다.
    _requestEndTurnOnCurrentActorDeath = false;
    Clear_DelayedEchoAttackSteps();
    _reactionController.Clear();

    if (_battleActionExe)
        _battleActionExe->Reset();

    _currentTurnActor = _turnQueue.top().hero;
    _battleTime = _turnQueue.top().nextTime;
    _turnQueue.pop();

    if (_currentTurnActor.statCom->Get_AliveStatus() == EAliveStatus::Dead)
    {
        _turnAction = {};
        _turnActionRequest = {};

        StartTurn();
        return;
    }


    _turnAction.ResetAction();
    _turnActionRequest = {};
    _turnActionRequest.actor = _currentTurnActor;

    _processedTurnStartStatuses.clear();
    _isWaitingTurnStartStatusHit = false;

    _buffSystem->ProcessTurnStart(_currentTurnActor);

    if (Try_BeginTurnStartStatusAnnounce())
        return;

    Continue_StartTurnAfterStatusEffects();
}

void BattleController::Create_BattleChar(const vector<HeroId>& heroIds, const vector<BattleMonsterSpawnDesc>& battleMonsterSpawnDescs)
{
    if (_battleSpawnRegistry)
        _battleSpawnRegistry->Clear_BattleReward();

    for (const HeroId heroId : heroIds)
    {
        const HeroSpawnDesc* spawnDesc = CLIENT->Find_HeroSpawnDesc(heroId);
        if (spawnDesc == nullptr)
            continue;

        Shared<GameObject> character = Spawn_Prefab::Spawn(
            spawnDesc->battlePrefabName,
            GAME->Current_LevelIndex(),
            L"BattleLayer"
        );

        CHECK_NULL(character);

        // Registry에 플레이어 등록
        _battleSpawnRegistry->Register_BattleHeroObject(character, GAME->Current_LevelIndex());

        const auto ply = dynamic_pointer_cast<Character_BattleBase>(character);

        BattleChar battleChar{};
        battleChar.heroId = heroId; // 배틀 종료 후 어떤 PW로 돌릴지 식별용
        battleChar.actor = ply;
        battleChar.statCom = character->Get_Component<CombatStatCom>();
        battleChar.mechanismCom = character->Get_Component<BattleMechanismCom>();

        battleChar.statCom->Set_PlayerType(To_PlayerCharacterType(heroId));

        if (battleChar.mechanismCom)
            battleChar.mechanismCom->Reset_ForHero(heroId); // 배틀 진입 시 메커니즘 초기화

        // 월드에서 세팅된 스탯들을 배틀턴 진입 시 반영
        const Shared<Player_WorldBase> worldHero = CLIENT->Find_Hero(heroId);
        const Shared<PlayerStatCom> worldStat = worldHero ? worldHero->Get_Component<PlayerStatCom>() : nullptr;
        const Shared<PlayerStatCom> battleStat = character->Get_Component<PlayerStatCom>();
        const Shared<SkillTreeCom> worldSkillTreeCom = worldHero ? worldHero->Get_Component<SkillTreeCom>() : nullptr;

        if (worldStat && battleStat)
            battleStat->Apply_BattleSyncSnapshot(worldStat->Create_BattleSyncSnapshot());

        if (battleStat)
        {
            if (battleStat->Get_PlayerRuntimeStat().CurEquippedWeaponId.empty())
            {
                const string characterId = string(magic_enum::enum_name(heroId));
                const vector<GameDataTable::WeaponDesc>* weapons = CLIENT->Get_WeaponDescs(characterId);

                if (weapons != nullptr && !weapons->empty())
                {
                    // 장착 무기없을 때에만 기본값
                    const GameDataTable::WeaponDesc& defaultWeapon = weapons->front();
                    battleStat->Set_EquippedWeapon(
                        defaultWeapon.weaponId,
                        CLIENT->Resolve_WeaponPower(characterId, defaultWeapon.weaponId));
                }
            }

            if (const auto battlePlayer = dynamic_pointer_cast<Player_BattleBase>(character))
                CHECK_FAILED(battlePlayer->Rebuild_WeaponPartsFromCurrentStat());

            battleStat->Set_BattleStartAp();
        }


        // World Skill Tree 읽어서 세팅
        const vector<string> equippedSkillKeys = worldStat ? worldStat->Get_SkillKeys() : vector<string>{};
        Configure_BattlePlayerSkills(heroId, battleChar.statCom, worldSkillTreeCom, equippedSkillKeys);

        PartObject* body = ply ? ply->Get_PartObject(L"Part_Body") : nullptr;
        battleChar.ModelCom = body ? body->Get_Component<ModelCom>() : nullptr; // body part에 붙은 실제 ModelCom를 사용한다.

        _heroes.push_back(battleChar);

        // battleChar.ModelCom->Set_RootMotionEnabled(false);
    }

    for (int i = 0; i < battleMonsterSpawnDescs.size(); ++i)
    {
        const BattleMonsterSpawnDesc& spawnDesc = battleMonsterSpawnDescs[i];
        Shared<GameObject> enemy = Spawn_Prefab::Spawn(
            spawnDesc.prefabName,
            GAME->Current_LevelIndex(),
            L"BattleLayer"
        );

        CHECK_NULL(enemy);

        // Registry에 몬스터 등록
        _battleSpawnRegistry->Register_BattleMonsterObject(enemy, GAME->Current_LevelIndex());

        const Shared<MonsterStatCom> monsterStat = enemy->Get_Component<MonsterStatCom>();
        if (monsterStat && spawnDesc.statSnapshot.valid)
            monsterStat->Apply_BattleSyncSnapshot(spawnDesc.statSnapshot);

        const auto ply = dynamic_pointer_cast<Character_BattleBase>(enemy);
        BattleChar battleChar{};
        battleChar.actor = ply;
        battleChar.statCom = enemy->Get_Component<CombatStatCom>();
        battleChar.mechanismCom = nullptr;

        PartObject* body = ply ? ply->Get_PartObject(L"Part_Body") : nullptr;
        battleChar.ModelCom = body ? body->Get_Component<ModelCom>() : nullptr; // 몬스터 종류와 무관하게 Part_Body의 ModelCom를 사용한다.

        _enemies.push_back(battleChar);

        // battleChar.ModelCom->Set_RootMotionEnabled(false);
    }
}

void BattleController::Build_InitialTurnQueue(bool isPlayerAdvantage)
{
    float heroOffset = {};
    float monsterOffset = {};

    // 첨 초기화
    _battleTime = 0;
    _nextTurnEntrySeq = 0;

    while (!_turnQueue.empty())
    {
        _turnQueue.pop();
    }

    // 선제 타격이면 몬스터를 늦추고, Player <-> Monster 충돌 진입이면 플레이어를 낮추기
    if (isPlayerAdvantage)
        monsterOffset = 5.f;
    else
        heroOffset = 5.f;

    for (const auto& hero : _heroes)
    {
        if (!hero.actor)
            continue;

        Push_TurnEntry(hero, 100.f / max(hero.statCom->Get_Speed(), 1.f) + heroOffset);
    }
    for (const auto& enemy : _enemies)
    {
        if (!enemy.actor)
            continue;

        Push_TurnEntry(enemy, 100.f / max(enemy.statCom->Get_Speed(), 1.f) + monsterOffset);
    }
}

void BattleController::Rebuild_TurnQueue(const BattleChar& revivedActor)
{
    if (revivedActor.actor == nullptr || revivedActor.statCom == nullptr)
        return;

    priority_queue<TurnEntry, vector<TurnEntry>, Compare> rebuiltQueue{};

    while (!_turnQueue.empty())
    {
        TurnEntry entry = _turnQueue.top();
        _turnQueue.pop();

        if (entry.hero.actor == nullptr || entry.hero.statCom == nullptr)
            continue;

        if (entry.hero.actor == revivedActor.actor)
            continue;

        if (entry.hero.statCom->Get_AliveStatus() != EAliveStatus::Alive)
            continue;

        rebuiltQueue.push(entry);
    }

    TurnEntry revivedEntry{};
    revivedEntry.hero = revivedActor;
    revivedEntry.nextTime = _battleTime + 100.f / max(revivedActor.statCom->Get_Speed(), 1.f);
    revivedEntry.orderSeq = _nextTurnEntrySeq++;

    rebuiltQueue.push(revivedEntry);
    _turnQueue = std::move(rebuiltQueue);
}

void BattleController::Push_TurnEntry(const BattleChar& actor, float nextTime)
{
    TurnEntry entry{};
    entry.hero = actor;
    entry.nextTime = nextTime;
    entry.orderSeq = _nextTurnEntrySeq++;

    _turnQueue.push(entry);
}

float BattleController::Resolve_PrefabGroundOffsetY(const Shared<GameObject>& actor) const
{
    if (Shared<Player_BattleBase> player = dynamic_pointer_cast<Player_BattleBase>(actor))
        return player->Get_BattleGroundOffsetY();

    if (Shared<Monster_BattleBase> monster = dynamic_pointer_cast<Monster_BattleBase>(actor))
        return monster->Get_BattleGroundOffsetY();

    return 0.f;
}

void BattleController::SetCharInitPos()
{
    auto normalizeAngleDegrees = [](float angle) -> float
        {
            while (angle > 180.f)
                angle -= 360.f;
            while (angle < -180.f)
                angle += 360.f;
            return angle;
        };

    auto projectToSpawnerLocal = [](const Vec3& worldPos, const Vec3& origin, const Vec3& localRight, const Vec3& localForward) -> Vec3
        {
            const Vec3 delta = worldPos - origin;
            const float localX = delta.x * localRight.x + delta.y * localRight.y + delta.z * localRight.z;
            const float localY = delta.y;
            const float localZ = delta.x * localForward.x + delta.y * localForward.y + delta.z * localForward.z;
            return Vec3(localX, localY, localZ);
        };

    Vec3 startPos = Vec3(145.f, 23.5f, 7.24f);
    Vec3 right = Vec3::Right;
    Vec3 forward = Vec3::Forward;

    if (_pendingBattleStartSpawnerId > 0)
    {
        bool foundSpawner = false;

        GAME->Visit_LevelObjects(
            GAME->Current_LevelIndex(),
            [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
            {
                if (foundSpawner)
                    return;

                if (layerTag != TEXT("Layer_BattleStartSpawner"))
                    return;

                Shared<BattleStartSpawner> spawner = dynamic_pointer_cast<BattleStartSpawner>(gameObject);
                if (spawner == nullptr)
                    return;

                if (spawner->Get_SpawnerId() != _pendingBattleStartSpawnerId)
                    return;

                Shared<TransformCom> spawnerTransform = spawner->Get_Transform();
                if (spawnerTransform == nullptr)
                    return;

                startPos = spawnerTransform->Get_WorldPosition();
                right = spawnerTransform->Get_WorldRight();
                forward = spawnerTransform->Get_WorldForward();
                foundSpawner = true;
            });
    }

    right.y = 0.f;
    if (right.LengthSquared() <= 0.0001f)
        right = Vec3::Right;
    else
        right.Normalize();

    forward.y = 0.f;
    if (forward.LengthSquared() <= 0.0001f)
        forward = Vec3::Forward;
    else
        forward.Normalize();

    _pendingBattlePlaneYaw = XMConvertToDegrees(atan2f(forward.x, forward.z));
    LOG_INFO(
        "[BattleLayout] spawnerId={} start=({:.3f},{:.3f},{:.3f}) right=({:.3f},{:.3f},{:.3f}) forward=({:.3f},{:.3f},{:.3f}) planeYaw={:.3f} startOffset=({:.3f},{:.3f},{:.3f})",
        _pendingBattleStartSpawnerId,
        startPos.x,
        startPos.y,
        startPos.z,
        right.x,
        right.y,
        right.z,
        forward.x,
        forward.y,
        forward.z,
        _pendingBattlePlaneYaw,
        _pendingBattleStartOffset.x,
        _pendingBattleStartOffset.y,
        _pendingBattleStartOffset.z);

    const Vec3 playerLocalPos[3] =
    {
        { -2.5f, 0.f, 0.f },
        {  0.f, 0.1f, 0.f },
        {  2.5f, 0.1f, 0.f },
    };
    const Vec3 enemyBasePos =
        startPos +
        (forward * 6.f) +
        right * _pendingBattleStartOffset.x +
        Vec3::Up * _pendingBattleStartOffset.y +
        forward * _pendingBattleStartOffset.z; // MW 프리팹에서 플레이어와 몬스터 사이 거리를 조절한다.
    const float battlePlaneY = startPos.y;

    Vec3 heroCenter = Vec3::Zero;
    uint32 heroCount = 0;

    for (size_t i = 0; i < _heroes.size() && i < 3; ++i)
    {
        const auto& hero = _heroes[i];
        if (!hero.actor)
            continue;

        const Vec3 placedPos =
            startPos +
            right * playerLocalPos[i].x +
            Vec3::Up * playerLocalPos[i].y +
            forward * playerLocalPos[i].z;

        Vec3 finalPos = placedPos;
        finalPos.y = battlePlaneY
            + playerLocalPos[i].y
            + Resolve_PrefabGroundOffsetY(hero.actor);

        hero.actor->Get_Transform()->Set_Position(finalPos);
        if (_battleSpawnRegistry)
            _battleSpawnRegistry->Update_SpawnPose(hero.actor);
        heroCenter += finalPos;
        ++heroCount;
    }

    if (heroCount > 0)
        heroCenter /= static_cast<float>(heroCount);

    Vec3 enemyCenter = Vec3::Zero;
    uint32 enemyCount = 0;

    for (size_t i = 0; i < _enemies.size(); ++i)
    {
        const auto& enemy = _enemies[i];
        if (!enemy.actor)
            continue;

        const Shared<TransformCom> transform = enemy.actor->Get_Transform();
        if (transform == nullptr)
            continue;

        Vec3 placedPos = enemyBasePos;
        Vec3 rotationOffset = Vec3::Zero;
        Vec3 localOffset = Vec3::Zero;

        if (i < _pendingBattleMonsterSpawnDescs.size())
        {
            localOffset = _pendingBattleMonsterSpawnDescs[i].offset;
            placedPos +=
                right * localOffset.x +
                forward * localOffset.z;
            rotationOffset = _pendingBattleMonsterSpawnDescs[i].rotation;
        }

        placedPos.y = battlePlaneY
            + Resolve_PrefabGroundOffsetY(enemy.actor)
            + _pendingBattleStartOffset.y
            + localOffset.y;

        transform->Set_Position(placedPos);
        if (Shared<Monster_BattleBase> monster = dynamic_pointer_cast<Monster_BattleBase>(enemy.actor))
            monster->Set_RevivePosition(placedPos);
        enemyCenter += placedPos;
        ++enemyCount;

        if (heroCount == 0)
            continue;

        Vec3 dir = heroCenter - placedPos;
        dir.y = 0.f;

        if (dir.LengthSquared() <= 0.0001f)
            continue;

        dir.Normalize();

        Vec3 rot = transform->Get_RotationEuler();
        rot.x += rotationOffset.x;
        rot.y = XMConvertToDegrees(atan2f(dir.x, dir.z)) + rotationOffset.y;
        rot.z += rotationOffset.z;
        transform->Set_RotationEuler(rot);

        if (_battleSpawnRegistry)
            _battleSpawnRegistry->Update_SpawnPose(enemy.actor);
    }

    if (enemyCount > 0)
    {
        enemyCenter /= static_cast<float>(enemyCount);

        for (size_t i = 0; i < _heroes.size() && i < 3; ++i)
        {
            const auto& hero = _heroes[i];
            if (!hero.actor)
                continue;

            const Shared<TransformCom> transform = hero.actor->Get_Transform();
            if (transform == nullptr)
                continue;

            Vec3 dir = enemyCenter - transform->Get_Position();
            dir.y = 0.f;

            if (dir.LengthSquared() <= 0.0001f)
                continue;

            dir.Normalize();

            Vec3 rot = transform->Get_RotationEuler();
            rot.y = XMConvertToDegrees(atan2f(dir.x, dir.z));
            transform->Set_RotationEuler(rot);

            if (_battleSpawnRegistry)
                _battleSpawnRegistry->Update_SpawnPose(hero.actor);
        }
    }

    for (size_t i = 0; i < _heroes.size() && i < 3; ++i)
    {
        const auto& hero = _heroes[i];
        if (hero.actor == nullptr || hero.actor->Get_Transform() == nullptr)
            continue;

        const Shared<TransformCom> transform = hero.actor->Get_Transform();
        const Vec3 worldPos = transform->Get_Position();
        const Vec3 localPos = projectToSpawnerLocal(worldPos, startPos, right, forward);
        const float relativeYaw = normalizeAngleDegrees(transform->Get_RotationEuler().y - _pendingBattlePlaneYaw);
        const Vec3 toEnemyCenterLocal = projectToSpawnerLocal(enemyCenter, worldPos, right, forward);

        LOG_INFO(
            "[BattleLayout][Hero{}] heroId={} world=({:.3f},{:.3f},{:.3f}) local=({:.3f},{:.3f},{:.3f}) yaw={:.3f} relativeYaw={:.3f} enemyCenterLocal=({:.3f},{:.3f},{:.3f})",
            i,
            static_cast<int>(hero.heroId),
            worldPos.x,
            worldPos.y,
            worldPos.z,
            localPos.x,
            localPos.y,
            localPos.z,
            transform->Get_RotationEuler().y,
            relativeYaw,
            toEnemyCenterLocal.x,
            toEnemyCenterLocal.y,
            toEnemyCenterLocal.z);
    }

    for (size_t i = 0; i < _enemies.size(); ++i)
    {
        const auto& enemy = _enemies[i];
        if (enemy.actor == nullptr || enemy.actor->Get_Transform() == nullptr)
            continue;

        const Shared<TransformCom> transform = enemy.actor->Get_Transform();
        const Vec3 worldPos = transform->Get_Position();
        const Vec3 localPos = projectToSpawnerLocal(worldPos, startPos, right, forward);
        const float relativeYaw = normalizeAngleDegrees(transform->Get_RotationEuler().y - _pendingBattlePlaneYaw);

        Vec3 localOffset = Vec3::Zero;
        if (i < _pendingBattleMonsterSpawnDescs.size())
            localOffset = _pendingBattleMonsterSpawnDescs[i].offset;

        LOG_INFO(
            "[BattleLayout][Enemy{}] prefab={} world=({:.3f},{:.3f},{:.3f}) local=({:.3f},{:.3f},{:.3f}) yaw={:.3f} relativeYaw={:.3f} spawnOffset=({:.3f},{:.3f},{:.3f})",
            i,
            enemy.actor->Get_SourcePrefabName().c_str(),
            worldPos.x,
            worldPos.y,
            worldPos.z,
            localPos.x,
            localPos.y,
            localPos.z,
            transform->Get_RotationEuler().y,
            relativeYaw,
            localOffset.x,
            localOffset.y,
            localOffset.z);
    }
}

void BattleController::Request_WidePresentationMode()
{
    if (!_battleTurnCamera)
        return;

    LOG_INFO("[BattleCamTrace] Request_WidePresentationMode");

    EvtBattlePresentation event{};
    event.cameraMode = BattleCameraMode::WIDE;
    event.subject = nullptr;
    GAME->Get_EventBus().Publish(event);
}

void BattleController::Request_EscapeBattle()
{
    if (!_isBattleActive || _battleEnded)
        return;

    if (!Is_PlayerTurnActive())
        return;

    if (Get_CurrentTurnState() != BattleTurnState::ChooseAction)
        return;

    _battleEnded = true; // 같은 프레임에 다른 종료 경로가 겹치지 않게 먼저 잠근다.
    OnExitBattleField(EBattleEndResult::Neutral);
}

vector<BattleChar> BattleController::Get_AliveEnemies()
{
    vector<BattleChar> alive;
    for (int i = 0; i < _enemies.size(); i++)
    {
        if (_enemies[i].statCom->Get_CurAliveState() == EAliveStatus::Alive)
            alive.push_back(_enemies[i]);
    }
    return alive;
}

vector<BattleChar> BattleController::Get_AliveHeroes()
{
    vector<BattleChar> alive;
    for (int i = 0; i < _heroes.size(); i++)
    {
        if (_heroes[i].statCom->Get_CurAliveState() == EAliveStatus::Alive)
            alive.push_back(_heroes[i]);
    }
    return alive;
}

bool BattleController::Will_NextScheduledTurnBePlayer(const BattleChar& currentActor) const
{
    auto queueCopy = _turnQueue;
    uint64 previewNextTurnEntrySeq = _nextTurnEntrySeq;

    if (currentActor.actor != nullptr &&
        currentActor.statCom != nullptr &&
        currentActor.statCom->Get_CurAliveState() == EAliveStatus::Alive)
    {
        TurnEntry nextEntry{};
        nextEntry.hero = currentActor;
        nextEntry.nextTime =
            _battleTime + 100.f / max(currentActor.statCom->Get_Speed(), 1.f);
        nextEntry.orderSeq = previewNextTurnEntrySeq++;
        queueCopy.push(nextEntry);
    }

    while (!queueCopy.empty())
    {
        const TurnEntry entry = queueCopy.top();
        queueCopy.pop();

        if (entry.hero.actor == nullptr || entry.hero.statCom == nullptr)
            continue;

        if (entry.hero.statCom->Get_CurAliveState() != EAliveStatus::Alive)
            continue;

        return entry.hero.statCom->Is_Player();
    }

    return false;
}

void BattleController::Handle_EditorStopRuntime()
{
    const bool hadBattleRuntime =
        _isBattleEntering ||
        _isWaitingBattleTransition ||
        _isBattleActive ||
        _isPendingVictorySlow ||
        _isWaitingVictoryConfirm;

    GAME->Get_ScreenFxManager()->Clear_ScreenEffect();
    CLIENT->Stop_BattleEnterTransition();
    GAME->Stop_WorldTimeSlow();
    GAME->Set_WorldTimeScale(1.f);
    GAME->Set_PlayerInputBlocked(false);

    Clear_BattleRuntimeState();

    _battleEnded = false;
    _isBattleActive = false;
    _isBattleEntering = false;
    _isWaitingBattleTransition = false;
    _isPendingVictorySlow = false;
    _isWaitingVictoryConfirm = false;
    _requestEndTurnOnCurrentActorDeath = false;
    _victorySlow.elapsed = 0.f;

    if (_battleSpawnRegistry)
    {
        _battleSpawnRegistry->Destroy_SpawnedObjects();
        _battleSpawnRegistry->Clear_SourceWorldObject();
    }

    _heroes.clear();
    _enemies.clear();

    _battleTurnCamera.reset();

    Clear_BattleEnterPending();
    _activeBattleFieldID = 0;

    // DOF_MODIFY 
    End_BattleDOFOverride();
    if (hadBattleRuntime)
        Restore_WorldCamera();

    if (_battleFSM)
        _battleFSM->Change_State(BattleTurnState::ChooseAction);

    //
    _firstAnimflag = false;
    _firstAnimCount = 0;
    _firstAnimTargetCount = 0;
}

//void BattleController::Handle_TargetPreviewChanged(const BattleChar* target)
//{
//    if(!_battleTurnCamera)
//    return;
//
//    BattleTurnCamera::BattleShotRequest request{};
//
//    if (target == nullptr || target->actor == nullptr)
//    {
//        request.mode = BattleCameraMode::WIDE;
//        request.subject = nullptr;
//    }
//    else
//    {
//        request.mode = BattleCameraMode::ENEMYFOCUS;
//        request.subject = target->actor;
//    }
//
//    _battleTurnCamera->Apply_ShotRequest(request);
//}

HRESULT BattleController::Ready_Delegates()
{
    _editorStopHandle = GAME->Get_DelegateHub().OnEditorStopRuntime.Add(
        [this]
        {
            Handle_EditorStopRuntime();
        }
    );

    _damageAppliedHandle = DamageSystem::OnDamageApplied.Add(
        this, &BattleController::Handle_DamageApplied);

    if (_battleTutorialController)
        _battleTutorialController->Bind_BattleController(GetSharedPtr<BattleController>()); // 튜토리얼 컨트롤러가 전투/QTE/리액션 이벤트를 구독한다.

    _reactionSucceededResultHandle =
        _reactionController.OnReactionSucceeded.Add(
            this, &BattleController::Handle_ReactionSucceededForResult);

    return S_OK;
}

void BattleController::Set_TargetPreview(const BattleChar& target)
{
    _targetPreview = target;
    _hasTargetPreview = target.actor != nullptr && target.statCom != nullptr;

    if (_hasTargetPreview)
        OnTargetPreviewChanged.Broadcast(&_targetPreview);
    else
        OnTargetPreviewChanged.Broadcast(nullptr);
}

void BattleController::Clear_TargetPreview()
{
    _targetPreview = {};
    _hasTargetPreview = false;
    OnTargetPreviewChanged.Broadcast(nullptr);
}

const BattleChar* BattleController::Get_TargetPreview() const
{
    if (!_hasTargetPreview)
        return nullptr;

    if (_targetPreview.actor == nullptr || _targetPreview.statCom == nullptr)
        return nullptr;

    return &_targetPreview;
}

bool BattleController::Ensure_EnemyTargetPreview()
{
    auto aliveEnemies = Get_AliveEnemies();

    if (aliveEnemies.empty())
    {
        Clear_TargetPreview();
        return false;
    }

    if (Find_EnemyTargetPreviewIndex(aliveEnemies) >= 0)
    {
        return true;
    }

    const size_t index = min<size_t>(1, aliveEnemies.size() - 1);
    Set_TargetPreview(aliveEnemies[index]);

    return true;
}

bool BattleController::Move_EnemyTargetPreview(int32 direction)
{
    if (direction == 0)
        return Ensure_EnemyTargetPreview();

    const vector<BattleChar> aliveEnemies = Get_AliveEnemies();

    if (aliveEnemies.empty())
    {
        Clear_TargetPreview();
        return false;
    }

    int32 currentIndex = Find_EnemyTargetPreviewIndex(aliveEnemies);

    if (currentIndex < 0)
        currentIndex = 0;

    const int32 nextIndex = currentIndex + direction;

    if (nextIndex < 0 || nextIndex >= static_cast<int32>(aliveEnemies.size()))
        return false;

    Set_TargetPreview(aliveEnemies[nextIndex]);
    return true;
}

void BattleController::Publish_TargetPreviewEnemyFocus() const
{
    const BattleChar* target = Get_TargetPreview();

    if (target == nullptr || target->actor == nullptr)
        return;

    EvtBattlePresentation event{};
    event.cameraMode = BattleCameraMode::ENEMYFOCUS;
    event.subject = target->actor;
    event.presetId = target->actor->Get_SourcePrefabName();

    GAME->Get_EventBus().Publish(event);
}

int32 BattleController::Find_EnemyTargetPreviewIndex(const vector<BattleChar>& aliveEnemies) const
{
    const BattleChar* preview = Get_TargetPreview();

    if (preview == nullptr || preview->actor == nullptr)
        return -1;

    for (int32 i = 0; i < static_cast<int32>(aliveEnemies.size()); ++i)
    {
        if (aliveEnemies[i].actor == preview->actor)
        {
            return i;
        }
    }

    return -1;
}

bool BattleController::Is_PlayerTurnActive() const
{
    if (!_isBattleActive || _battleEnded)
        return false;

    if (_isBattleEntering || _isWaitingBattleTransition)
        return false;

    if (_currentTurnActor.actor == nullptr || _currentTurnActor.statCom == nullptr)
        return false;

    return _currentTurnActor.statCom->Is_Player();
}

BattleTurnState BattleController::Get_CurrentTurnState() const
{
    if (_battleFSM == nullptr)
        return BattleTurnState::None;

    return _battleFSM->Get_CurrentPlayerState();
}

void BattleController::Execute_BaseAttack()
{
    auto& action = Get_TurnAction();
    const BattleChar& actor = action.Get_CurChar();

    //기본스킬도 특이하기때문에 스킬로
    LOG_DEBUG(
        "Execute_BaseAttack: actor={}, target={}, script={}",
        actor.actor ? String::ToString(actor.actor->Get_Name()).c_str() : "null",
        action.GetCurStepTarget().empty() ? "null" : String::ToString(action.GetCurStepTarget()[0].actor->Get_Name()).c_str(),
        action.GetSkillData().scriptClassName.c_str()
    );

    // auto script = Create_Class::SkillScript<Base_Gustave_Attack>().Create();
    _curScript = Create_Class::SkillScript(action.GetSkillData().scriptClassName).Create();

    if (actor.ModelCom == nullptr)
    {
        LOG_ERROR("Execute_BaseAttack: actor.ModelCom is null");
        EndTurn();
        return;
    }
    if (action.GetSkillData().AnimPhases.empty())
    {
        LOG_ERROR("Execute_BaseAttack: no animationKey");
        EndTurn();
        return;
    }

    action.InitActionDirection();

    // 기본 공격은 AP + 1
    if (action.Get_CurChar().statCom)
    {
        const uint32 beforeAp = action.Get_CurChar().statCom->Get_CurAP();

        action.Get_CurChar().statCom->Add_AP(1);

        const uint32 afterAp = action.Get_CurChar().statCom->Get_CurAP();
        const uint32 gainedAp = afterAp - beforeAp;

        if (gainedAp > 0)
        {
            // 최대 AP라 실제 증가가 없으면 안띄우기
            OnBattleApChanged.Broadcast(action.Get_CurChar(), gainedAp, EAPChangeReason::BaseAttack);
        }
    }

    _isActionExecuting = true;
    _battleActionExe->BeginAction(action);
}

void BattleController::Execute_Skill()
{
    auto& action = Get_TurnAction();
    const BattleChar& actor = action.Get_CurChar();

    if (actor.ModelCom == nullptr)
    {
        LOG_ERROR("Execute_Skill: actor.ModelCom is null");
        EndTurn();
        return;
    }

    if (actor.statCom == nullptr)
    {
        LOG_ERROR("Execute_Skill: actor.statCom is null");
        EndTurn();
        return;
    }

    if (action.GetSkillData().AnimPhases.empty())
    {
        LOG_ERROR("Execute_Skill: no animationKey");
        EndTurn();
        return;
    }

    _curScript = Create_Class::SkillScript(action.GetSkillData().scriptClassName).Create();
    if (_curScript == nullptr)
    {
        LOG_ERROR("Execute_Skill: script create failed. script={}", action.GetSkillData().scriptClassName.c_str());
        EndTurn();
        return;
    }

    const uint32 apCost = action.GetSkillData().apCost > 0
        ? static_cast<uint32>(action.GetSkillData().apCost)
        : 0u;

    const bool isGradientSkill =
        actor.statCom->Is_Player() &&
        GradientSkill::Is_GradientSkill(actor.heroId, action.GetSkillData().skillKey);

    if (apCost > 0 && actor.statCom->Get_CurAP() < apCost)
    {
        LOG_DEBUG("Execute_Skill: not enough AP. skill={}, curAP={}, cost={}",
            action.GetSkillData().skillKey.c_str(),
            actor.statCom->Get_CurAP(),
            apCost);

        Clear_CurrentScript();

        if (actor.statCom->Is_Player() && _battleFSM)
            _battleFSM->Change_State(BattleTurnState::ChooseSkill); // 기존 AP 부족 복귀 흐름 유지
        else
            EndTurn();

        return;
    }

    // 귀찮아서 여기에. 구스타브 과충전 10스택
    if (action.GetSkillData().skillKey == "UnleashedCharge" &&
        actor.mechanismCom != nullptr &&
        !actor.mechanismCom->Can_UseGustaveOvercharge())
    {
        Clear_CurrentScript();

        if (actor.statCom->Is_Player() && _battleFSM)
            _battleFSM->Change_State(BattleTurnState::ChooseSkill);
        else
            EndTurn();

        return;
    }

    if (isGradientSkill && !actor.statCom->Can_UseGradientGauge())
    {
        LOG_DEBUG("Execute_Skill: gradient gauge is empty. skill={}",
            action.GetSkillData().skillKey.c_str());

        Clear_CurrentScript();

        if (_battleFSM)
            _battleFSM->Change_State(BattleTurnState::ChooseGradient); // Gradient 선택 상태로 되돌린다.
        else
            EndTurn();

        return;
    }

    if (apCost > 0)
        actor.statCom->Try_ConsumeAP(apCost);

    if (isGradientSkill)
        actor.statCom->Try_ConsumeGradientGauge(GradientSkill::GaugeCostPerUse);

    action.InitActionDirection();

    _isActionExecuting = true;
    _battleActionExe->BeginAction(action);
}

void BattleController::Execute_Item()
{
    auto& action = Get_TurnAction();
    const BattleChar& actor = action.Get_CurChar();

    // 아이템은 광역 콤보 등이 없으니 다 초기 타겟으로 함 
    if (action.GetInitTarget().actor == nullptr)
    {
        assert("Execute_Item_GetInitTarget==0");
    }
    ;
    CHECK_NULL(actor.statCom);
    const Item_Data& itemData = action.GetItemData();

    switch (itemData.itemType)
    {
    case BattleItemType::HealingTint:
    {
        const BattleChar& target = action.GetInitTarget();

        if (target.statCom->Get_AliveStatus() != EAliveStatus::Alive)
            return; // 치유 틴트는 살아있는 대상에게만 적용한다.

        auto& runtimeStat = target.statCom->Get_RuntimeStat();
        const float beforeHp = runtimeStat.CurHp;

        DamageSystem::Heal(
            target.statCom->Get_StatInfo().maxHp *
            static_cast<float>(itemData.healHp) * 0.01f, target);

        // 힐 이팩트
        if (runtimeStat.CurHp > beforeHp && target.actor != nullptr)
        {
            PlayEffectDesc effectDesc{};
            effectDesc.effectName = kHealTintEffect;
            effectDesc.spawnBasisTarget = target.actor;
            effectDesc.localOffset = Vec3(0.f, 0.f, 0.f);
            effectDesc.scale = Vec3(1.f, 1.f, 1.f);

            CLIENT->Play_Effect(effectDesc);
        }
    }

    break;

    case BattleItemType::EnergyTint:
    {
        const BattleChar& target = action.GetInitTarget();

        if (target.statCom->Get_AliveStatus() != EAliveStatus::Alive)
            return; // 에너지 틴트는 살아있는 대상에게만 적용한다.

        // 에너지 틴트 이팩트
        {
            PlayEffectDesc effectDesc{};
            effectDesc.effectName = kEnergyTintEffect;
            effectDesc.spawnBasisTarget = target.actor;
            effectDesc.localOffset = Vec3(0.f, 0.f, 0.f);
            effectDesc.scale = Vec3(1.f, 1.f, 1.f);

            CLIENT->Play_Effect(effectDesc);
        }

        const uint32 beforeAp = target.statCom->Get_CurAP();

        target.statCom->Add_AP(itemData.restoreAp);

        const uint32 gainedAp = target.statCom->Get_CurAP() - beforeAp;
        if (gainedAp > 0)
            OnBattleApChanged.Broadcast(target, gainedAp, EAPChangeReason::ConsumableItem);
    }

    break;

    case BattleItemType::ReviveTint:
        if (action.GetInitTarget().statCom->Get_AliveStatus() != EAliveStatus::Dead)
            return; // 부활 틴트는 죽어있는 대상에게만 적용한다.

        if (action.GetInitTarget().statCom->Is_ReviveBlocked())
            return;

        // 에너지 틴트 이팩트
        {
            PlayEffectDesc effectDesc{};
            effectDesc.effectName = kReviveTintEffect;
            effectDesc.spawnBasisTarget = action.GetInitTarget().actor;
            effectDesc.localOffset = Vec3(0.f, 0.f, 0.f);
            effectDesc.scale = Vec3(1.f, 1.f, 1.f);

            CLIENT->Play_Effect(effectDesc);
        }

        //Rebuild_TurnQueue(action.GetInitTarget());
        Revive_BattleMon(action.GetInitTarget(), static_cast<float>(itemData.reviveHp) * 0.01f);
        break;

    default:
        return;
    }

    CLIENT->Consume_Item(itemData.itemType);
    Clear_TargetPreview();
}


bool BattleController::Should_UpdateRuntime() const
{
    return
        _isBattleEntering ||
        _isWaitingBattleTransition ||
        _isBattleActive ||
        _isPendingVictorySlow ||
        _isWaitingVictoryConfirm;
}

void BattleController::Revive_BattleMon(const BattleChar& battleChar, float MultipleHp)
{
    if (battleChar.statCom == nullptr)
        return;

    if (battleChar.statCom->Is_ReviveBlocked())
        return;

    battleChar.statCom->Set_AliveStatus(EAliveStatus::Alive); // 먼저 부활 상태로 복구한다.
    Restore_BattleTargetCollision(battleChar); // 부활 직후 다시 충돌 가능 상태로 전환

    if (battleChar.actor)
    {
        if (Shared<Monster_BattleBase> monster = dynamic_pointer_cast<Monster_BattleBase>(battleChar.actor))
        {
            if (monster->Has_RevivePosition())
                monster->Get_Transform()->Set_Position(monster->Get_RevivePosition());
        }

        for (const auto& [partTag, partObject] : battleChar.actor->Get_PartObjects())
        {
            UNREFERENCED_PARAMETER(partTag);

            if (partObject == nullptr)
                continue;

            partObject->Set_Visible(true);

            if (const Shared<EffectCom> partEffect = partObject->Get_Component<EffectCom>())
                partEffect->Start_SwitchDissolve(0.35f, "T_Perlin_Noise_M");
        }

        battleChar.actor->Set_Visible(true);

        if (const Shared<EffectCom> actorEffect = battleChar.actor->Get_Component<EffectCom>())
            actorEffect->Start_SwitchDissolve(0.35f, "T_Perlin_Noise_M");

    }

    DamageSystem::Heal(battleChar.statCom->Get_StatInfo().maxHp * MultipleHp, battleChar);


    if (battleChar.ModelCom)
    {
        if (!battleChar.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Revive), 0.2f, false))
        {
            BattleAnimUtils::Play_BattleIdle(
                battleChar,
                0.2f); // Revive 매핑이 없으면 부활된 대상 기준 idle로 복귀한다.
        }
    }

    Rebuild_TurnQueue(battleChar);
}

void BattleController::Apply_BaseAttackAP(const BattleChar& actor)
{
    CHECK_NULL(actor.statCom);

    // 기본 공격시 AP 1회복
    actor.statCom->Add_AP(1);
}

bool BattleController::Apply_FreeAimShootAP(const BattleChar& actor)
{
    CHECK_NULL(actor.statCom, false);

    return actor.statCom->Try_ConsumeAP(1);
}

void BattleController::Apply_VictoryRewardExp(uint32 rewardExp)
{
    if (rewardExp == 0)
        return;

    for (const BattleChar& hero : _heroes)
    {
        const Shared<PlayerStatCom> playerStat =
            dynamic_pointer_cast<PlayerStatCom>(hero.statCom);

        if (playerStat == nullptr)
            continue;

        playerStat->Add_Exp(rewardExp);
    }
}

void BattleController::Play_EnemyTargetedStart(const vector<BattleChar>& targets)
{
    _enemyTurnTargetedActors.clear();

    for (const BattleChar& target : targets)
    {
        if (target.actor == nullptr || target.ModelCom == nullptr || target.statCom == nullptr)
            continue;

        if (!target.statCom->Is_Player())
            continue;

        if (target.statCom->Get_AliveStatus() == EAliveStatus::Dead)
            continue;

        bool alreadyAdded = false;
        for (const BattleChar& existing : _enemyTurnTargetedActors)
        {
            if (existing.actor == target.actor)
            {
                alreadyAdded = true;
                break;
            }
        }

        if (alreadyAdded)
            continue;

        _enemyTurnTargetedActors.push_back(target);
    }

    if (_enemyTurnTargetedActors.empty())
        return;

    const BattleChar& attacker = Get_TurnActionRequest().actor;
    if (attacker.actor != nullptr)
    {
        const auto attackerTransform = attacker.actor->Get_Transform();

        if (attackerTransform != nullptr)
        {
            for (const BattleChar& target : _enemyTurnTargetedActors)
            {
                const auto targetTransform = target.actor->Get_Transform();
                if (targetTransform == nullptr)
                    continue;

                Vec3 dir = attackerTransform->Get_Position() - targetTransform->Get_Position();
                dir.y = 0.f;

                if (dir.LengthSquared() > 0.0001f)
                {
                    dir.Normalize();

                    Vec3 rot = targetTransform->Get_RotationEuler();
                    rot.y = XMConvertToDegrees(atan2f(dir.x, dir.z));
                    targetTransform->Set_RotationEuler(rot);
                }
            }
        }
    }

    for (const BattleChar& hero : _heroes)
    {
        if (hero.actor == nullptr)
            continue;

        bool isTargeted = false;
        for (const BattleChar& target : _enemyTurnTargetedActors)
        {
            if (target.actor == hero.actor)
            {
                isTargeted = true;
                break;
            }
        }

        if (isTargeted)
            continue;

        BattleAnimUtils::Play_BattleIdle(hero, 0.2f);
    }

    for (const BattleChar& target : _enemyTurnTargetedActors)
    {
        if (!target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Targeted_Start), 0.2f, false))
        {
            if (!target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Targeted_Idle), 0.2f, true))
                BattleAnimUtils::Play_BattleIdle(target, 0.2f);
        }
    }
}

void BattleController::Play_EnemyTargetedEnd()
{
    if (_enemyTurnTargetedActors.empty())
        return;

    const vector<BattleChar> targetedActors = _enemyTurnTargetedActors;
    _enemyTurnTargetedActors.clear();

    for (const BattleChar& target : targetedActors)
    {
        if (target.actor == nullptr || target.ModelCom == nullptr || target.statCom == nullptr)
            continue;

        if (target.statCom->Get_AliveStatus() == EAliveStatus::Dead)
            continue;

        Request_BattleSpawnRotationReturn(target, 0.2f);

        if (!target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Targeted_End), 0.2f, false))
            BattleAnimUtils::Play_BattleIdle(target, 0.2f);
    }
}

void BattleController::Request_BattleSpawnRotationReturn(const BattleChar& target, float duration)
{
    if (target.actor == nullptr || target.statCom == nullptr)
        return;

    if (target.statCom->Get_AliveStatus() == EAliveStatus::Dead)
        return;

    if (_battleSpawnRegistry == nullptr)
        return;

    const BattleSpawnRegistry::BattleEntry* spawnEntry =
        _battleSpawnRegistry->Find_SpawnEntry(target.actor);

    if (spawnEntry == nullptr)
        return;

    const Shared<TransformCom> transform = target.actor->Get_Transform();
    if (transform == nullptr)
        return;

    for (BattleSpawnRotationReturn& runtime : _battleSpawnRotationReturns)
    {
        if (runtime.target.actor == target.actor)
        {
            runtime.target = target;
            runtime.startRotation = transform->Get_RotationEuler();
            runtime.goalRotation = spawnEntry->spawnRotation;
            runtime.elapsed = 0.f;
            runtime.duration = max(duration, 0.001f);
            return;
        }
    }

    BattleSpawnRotationReturn runtime{};
    runtime.target = target;
    runtime.startRotation = transform->Get_RotationEuler();
    runtime.goalRotation = spawnEntry->spawnRotation;
    runtime.elapsed = 0.f;
    runtime.duration = max(duration, 0.001f);

    _battleSpawnRotationReturns.push_back(runtime);
}

void BattleController::Update_BattleSpawnRotationReturns(float timeDelta)
{
    for (size_t i = 0; i < _battleSpawnRotationReturns.size();)
    {
        BattleSpawnRotationReturn& runtime = _battleSpawnRotationReturns[i];

        if (runtime.target.actor == nullptr ||
            runtime.target.statCom == nullptr ||
            runtime.target.statCom->Get_AliveStatus() == EAliveStatus::Dead)
        {
            _battleSpawnRotationReturns.erase(_battleSpawnRotationReturns.begin() + i);
            continue;
        }

        const Shared<TransformCom> transform = runtime.target.actor->Get_Transform();
        if (transform == nullptr)
        {
            _battleSpawnRotationReturns.erase(_battleSpawnRotationReturns.begin() + i);
            continue;
        }

        runtime.elapsed += max(0.f, timeDelta);

        const float ratio = clamp(runtime.elapsed / max(runtime.duration, 0.001f), 0.f, 1.f);

        Vec3 nextRotation = runtime.startRotation;

        float deltaYaw = runtime.goalRotation.y - runtime.startRotation.y;
        while (deltaYaw > 180.f)
            deltaYaw -= 360.f;
        while (deltaYaw < -180.f)
            deltaYaw += 360.f;

        nextRotation.y = runtime.startRotation.y + deltaYaw * ratio;
        transform->Set_RotationEuler(nextRotation);

        if (ratio >= 1.f)
        {
            transform->Set_RotationEuler(runtime.goalRotation);
            _battleSpawnRotationReturns.erase(_battleSpawnRotationReturns.begin() + i);
            continue;
        }

        ++i;
    }
}

void BattleController::Begin_FreeAimShot(const BattleAction& shotAction)
{
    if (_isFreeAimShotPlaying)
        return;

    if (shotAction.Get_CurChar().actor == nullptr)
        return;

    _freeAimShotAction = shotAction;
    //_isFreeAimShotPlaying = false;

    if (_freeAimShotAction.Get_CurChar().ModelCom == nullptr)
    {
        _freeAimShotAction = {};
        return;
    }

    if (!Apply_FreeAimShootAP(_freeAimShotAction.Get_CurChar()))
    {
        _freeAimShotAction = {};
        return;
    }

    // Free Aim은 히트스캔 즉발데미지
    if (_freeAimShotAction.GetInitTarget().actor != nullptr && _freeAimShotAction.GetInitTarget().statCom != nullptr)
    {
        constexpr float freeAimDamage = 20.f; // 우선 테스트용 데미지

        const BattleChar& attacker = _freeAimShotAction.Get_CurChar();
        const BattleChar& target = _freeAimShotAction.GetInitTarget();
        const bool hadShieldBeforeShot = target.statCom->Get_CurrentShield() > 0;

        DamageSystem::ApplyDamage(freeAimDamage, attacker, target, AttackType::FreeAimLight);

        // 약점 파츠를 맞췄을 때만 추가딜
        if (!hadShieldBeforeShot && _hasFreeAimHitTarget && _freeAimHitTarget.target.actor == target.actor)
            Try_ApplyFreeAimWeakPointBonus(_freeAimHitTarget, attacker);

        CLIENT->Notify_FreeAimShotHit();
    }

    _freeAimShotAction.InitActionDirection();

    _freeAimShotAction.Get_CurChar().ModelCom->Set_AnimationByState(
        ETOI(BattleAnimType::FreeAim_Shoot),
        0.1f,
        false);

    _isFreeAimShotPlaying = true;
}

void BattleController::Finish_FreeAimShot()
{
    Clear_CurrentScript();
    //Clear_TargetPreview();

    if (_freeAimShotAction.Get_CurChar().ModelCom)
    {
        // Free Aim 발사 후에는 Base Idle로 풀지 않고 다시 조준 idle 자세로 복귀. 턴 종료가 안됐으니까
        _freeAimShotAction.Get_CurChar().ModelCom->Set_AnimationByState(
            ETOI(BattleAnimType::FreeAim_Idle),
            0.1f,
            true);
    }

    _isFreeAimShotPlaying = false;
    _freeAimShotAction = {};

}

void BattleController::OnActionEndNotify()
{
    if (_isPendingVictorySlow || _battleEnded)
        return; // 마지막 몬스터 사망 후에는 공격자의 다음 phase로 넘기지 않는다.

    if (_isFreeAimShotPlaying)
    {
        Finish_FreeAimShot();

        return;
    }

    auto& action = Get_TurnAction();
    _battleActionExe->OnActionEndNotify(action);
}

void BattleController::Spawn_BattleChar(bool IsHero, string prefabName, Vec3 pos, float yawDeg)
{
    if (IsHero)
    {
        if (_heroes.size() == 3)
            return;


        Shared<GameObject> hero = Spawn_Prefab::Spawn(
            prefabName,
            GAME->Current_LevelIndex(),
            L"BattleLayer"
        );

        CHECK_NULL(hero);

        if (_battleSpawnRegistry)
            _battleSpawnRegistry->Register_BattleHeroObject(hero, GAME->Current_LevelIndex());

        const auto ply = dynamic_pointer_cast<Character_BattleBase>(hero);
        BattleChar battleChar{};
        battleChar.actor = ply;
        battleChar.statCom = hero->Get_Component<CombatStatCom>();

        PartObject* body = ply ? ply->Get_PartObject(L"Part_Body") : nullptr;
        battleChar.ModelCom = body ? body->Get_Component<ModelCom>() : nullptr; // 몬스터 종류와 무관하게 Part_Body의 ModelCom를 사용한다.

        _heroes.push_back(battleChar);

        battleChar.actor->Get_Transform()->Set_Position(pos);
        Vec3 rot = battleChar.actor->Get_Transform()->Get_RotationEuler();
        rot.y = yawDeg;
        battleChar.actor->Get_Transform()->Set_RotationEuler(rot);

        Push_TurnEntry(battleChar, _battleTime + 100.f / max(battleChar.statCom->Get_Speed(), 1.f));
    }
    else
    {
        if (_enemies.size() == 3)
            return;

        Shared<GameObject> enemy = Spawn_Prefab::Spawn(
            prefabName,
            GAME->Current_LevelIndex(),
            L"BattleLayer"
        );

        CHECK_NULL(enemy);

        if (_battleSpawnRegistry)
            _battleSpawnRegistry->Register_BattleMonsterObject(enemy, GAME->Current_LevelIndex());

        const auto ply = dynamic_pointer_cast<Character_BattleBase>(enemy);
        BattleChar battleChar{};
        battleChar.actor = ply;
        battleChar.statCom = enemy->Get_Component<CombatStatCom>();

        PartObject* body = ply ? ply->Get_PartObject(L"Part_Body") : nullptr;
        battleChar.ModelCom = body ? body->Get_Component<ModelCom>() : nullptr; // 몬스터 종류와 무관하게 Part_Body의 ModelCom를 사용한다.
        battleChar.actor->Get_Transform()->Set_Position(pos);
        Vec3 rot = battleChar.actor->Get_Transform()->Get_RotationEuler();
        rot.y = yawDeg;
        battleChar.actor->Get_Transform()->Set_RotationEuler(rot);
        if (Shared<Monster_BattleBase> monster = dynamic_pointer_cast<Monster_BattleBase>(battleChar.actor))
            monster->Set_RevivePosition(pos);

        _enemies.push_back(battleChar);


        Push_TurnEntry(battleChar, _battleTime + 100.f / max(battleChar.statCom->Get_Speed(), 1.f));
    }
}

vector<BattleChar> BattleController::Get_TurnOrderPreview(uint32 maxCount) const
{
    vector<BattleChar> preview{};
    if (maxCount == 0)
        return preview;

    auto queueCopy = _turnQueue;
    uint64 previewNextTurnEntrySeq = _nextTurnEntrySeq;

    if (_currentTurnActor.actor != nullptr && _currentTurnActor.statCom != nullptr)
    {
        preview.push_back(_currentTurnActor);

        // 현재 턴 actor도 다시 들어와야해서 preview 큐에 다시
        if (_currentTurnActor.statCom->Get_AliveStatus() == EAliveStatus::Alive)
        {
            TurnEntry nextEntry{};
            nextEntry.hero = _currentTurnActor;
            nextEntry.nextTime =
                _battleTime + 100.f / max(_currentTurnActor.statCom->Get_Speed(), 1.f);
            nextEntry.orderSeq = previewNextTurnEntrySeq++;

            queueCopy.push(nextEntry);
        }
    }

    while (!queueCopy.empty() && preview.size() < maxCount)
    {
        TurnEntry entry = queueCopy.top();
        queueCopy.pop(); // 위에꺼먼저 삭제

        if (entry.hero.actor == nullptr || entry.hero.statCom == nullptr)
            continue;

        if (entry.hero.statCom->Get_AliveStatus() != EAliveStatus::Alive)
            continue;

        preview.push_back(entry.hero);

        entry.nextTime += 100.f / max(entry.hero.statCom->Get_Speed(), 1.f);
        entry.orderSeq = previewNextTurnEntrySeq++;
        queueCopy.push(entry);
    }

    return preview;
}

void BattleController::Confirm_Victory()
{
    if (!_isWaitingVictoryConfirm)
        return;

    BattleFollowupDesc followupDesc{};
    const bool hasFollowupBattle =
        CLIENT->Get_BattleFollowupManager() != nullptr &&
        CLIENT->Get_BattleFollowupManager()->Consume_PendingFollowup(followupDesc);

    _isPendingVictorySlow = false;
    _isWaitingVictoryConfirm = false;
    _victorySlow.elapsed = 0.f;

    _battleEnded = false;
    _isBattleActive = false;
    _isBattleEntering = false;
    _isWaitingBattleTransition = false;

    GAME->Stop_WorldTimeSlow();
    GAME->Set_WorldTimeScale(1.f);
    GAME->Set_PlayerInputBlocked(false);

    // DOF_MODIFY 
    End_BattleDOFOverride();

    if (!hasFollowupBattle)
    {
        Restore_PreBattleBGM();
        Restore_WorldCamera(); // 후속 전투가 없을 때만 월드 카메라로 복구한다.
    }

    Clear_BattleRuntimeState();

    // 배틀 플레이어, 몬스터 삭제
    if (_battleSpawnRegistry)
    {
        _battleSpawnRegistry->Destroy_SpawnedObjects();

        if (hasFollowupBattle)
            _battleSpawnRegistry->Clear_SourceWorldObject(); // 후속 전투가 같은 MW source를 다시 넘겨받는다.
        else
            _battleSpawnRegistry->Destroy_SourceWorldObject();
    }

    _heroes.clear();
    _enemies.clear();

    _battleTurnCamera.reset(); // 다음 전투는 현재 레벨/Camera_Manager 기준으로 다시 BattleTurnCamera를 잡는다.

    Clear_BattleEnterPending();
    _activeBattleFieldID = 0;

    // 초기 애님 관련 변수 초기화
    _firstAnimflag = false;
    _firstAnimCount = 0;
    _firstAnimTargetCount = 0;

    if (hasFollowupBattle)
    {
        OnEnterBattleField(
            followupDesc.battleFieldId,
            followupDesc.isPlayerAdvantage,
            followupDesc.heroIds,
            followupDesc.monsterSpawnDescs,
            followupDesc.sourceWorldObject,
            true); // 2초 슬로우 걸지말고 바로 진입
    }
}

void BattleController::Handle_DamageApplied(const DamageAppliedInfo& info)
{
    if (_battleResultRecoder)
        _battleResultRecoder->Record_DamageApplied(info);

    if (info.damage > 0.f && info.target.actor)
    {
        const bool enteredBreak =
            info.target.actor->Try_ApplyBattleBreakOnDamage(info);

        _reactionController.Notify_TargetDamaged(info.target);

        if (!enteredBreak)
            HitAction(info.target, info.attackType, info.isCritical);
    }

    if (info.damage > 0.f &&
        info.target.actor != nullptr &&
        info.target.statCom != nullptr &&
        info.target.statCom->Get_AliveStatus() == EAliveStatus::Dead)
    {
        // 충돌체 끄기
        Handle_BattleTargetDeathCollision(info.target);
    }

    Try_RequestEndTurnOnCurrentActorDeath(info.target, info.damage);
    Try_BeginVictorySlowOnDamage(info.target, info.damage);
}

void BattleController::Try_RequestEndTurnOnCurrentActorDeath(const BattleChar& damagedTarget, float damage)
{
    if (damage <= 0.f)
        return;

    if (!_isActionExecuting)
        return;

    if (damagedTarget.actor == nullptr || damagedTarget.statCom == nullptr)
        return;

    if (_currentTurnActor.actor != damagedTarget.actor)
        return; // 맞은 대상이 현재 턴 액터일 때만 현재 액션을 끊기

    if (damagedTarget.statCom->Get_AliveStatus() != EAliveStatus::Dead)
        return;

    _requestEndTurnOnCurrentActorDeath = true;
}

void BattleController::Try_BeginVictorySlowOnDamage(const BattleChar& damagedTarget, float damage)
{
    if (_battleEnded || _isPendingVictorySlow || _isWaitingVictoryConfirm)
        return;

    if (damage <= 0.f)
        return;

    if (damagedTarget.actor == nullptr || damagedTarget.statCom == nullptr)
        return;

    if (damagedTarget.statCom->Is_Player())
        return;

    if (damagedTarget.statCom->Get_AliveStatus() != EAliveStatus::Dead)
        return;

    //if (damagedTarget.ModelCom)
    //    HitAction(damagedTarget,);

    Clear_TargetPreview();

    if (!Can_TriggerVictory())
        return;

    _isPendingVictorySlow = true;
    _victorySlow.elapsed = 0.f;

    GAME->Slow_WorldTime(_victorySlow.worldTimeScale, _victorySlow.duration);
}

bool BattleController::Can_TriggerVictory() const
{
    if (_enemies.empty())
        return false;

    for (const BattleChar& enemy : _enemies)
    {
        if (enemy.statCom == nullptr)
            continue;

        if (enemy.statCom->Get_AliveStatus() != EAliveStatus::Dead)
            return false;
    }

    return true;
}

void BattleController::Set_FreeAimHitTarget(const BattleFreeAimHitTarget& target)
{
    if (!target.Is_Valid())
    {
        Clear_FreeAimHitTarget();
        return;
    }

    _freeAimHitTarget = target;
    _hasFreeAimHitTarget = true;
}

void BattleController::Clear_FreeAimHitTarget()
{
    _freeAimHitTarget = {};
    _hasFreeAimHitTarget = false;
}

void BattleController::Queue_DelayedEchoAttackStep(float delaySec, const BattleChar& attacker,
    const vector<BattleChar>& targets, const BattleChar& reactionTarget, uint32 sourceStepIndex, uint32 consumeKey,
    AttackType attackType, float powerMultiplier)
{
    if (targets.empty())
        return;

    DelayedEchoAttackStep step{};
    step.remainingSec = max(0.f, delaySec);
    step.attacker = attacker;
    step.targets = targets;
    step.reactionTarget = reactionTarget;
    step.sourceStepIndex = sourceStepIndex;
    step.consumeKey = consumeKey;
    step.attackType = attackType;
    step.powerMultiplier = powerMultiplier;

    _delayedEchoAttackSteps.push_back(step);
}

bool BattleController::Is_QTEActive() const
{
    return _qteController != nullptr && _qteController->Is_Active();
}

bool BattleController::Has_LastQTEResult() const
{
    return _qteController != nullptr && _qteController->Has_LastResult();
}

BattleQTEResult BattleController::Get_LastQTEResult() const
{
    if (_qteController == nullptr)
        return BattleQTEResult::None;

    return _qteController->Get_LastResult();
}

void BattleController::Handle_BattleTargetDeathCollision(const BattleChar& deadTarget)
{
    if (deadTarget.actor == nullptr)
        return;

    deadTarget.actor->Set_BattleColliderActive(false);

    // 죽은 동안 약점 파츠 query/effect가 남지 않게 본체 owner가 같이 정리
    if (const Shared<Monster_BattleBase> monster = dynamic_pointer_cast<Monster_BattleBase>(deadTarget.actor))
        monster->Hide_FreeAimWeakPoint();

    if (_hasTargetPreview && _targetPreview.actor == deadTarget.actor)
        Clear_TargetPreview();

    if (_hasFreeAimHitTarget && _freeAimHitTarget.target.actor == deadTarget.actor)
        Clear_FreeAimHitTarget();
}

void BattleController::Restore_BattleTargetCollision(const BattleChar& revivedTarget)
{
    if (revivedTarget.actor == nullptr)
        return;

    revivedTarget.actor->Set_BattleColliderActive(true);

    // 부활하면 약점도 다시 세팅
    if (const Shared<Monster_BattleBase> monster = dynamic_pointer_cast<Monster_BattleBase>(revivedTarget.actor))
        monster->Reset_FreeAimWeakPoint();
}

void BattleController::Apply_Buff(const BattleChar& target, Buffs buffId, uint32 stack, int32 durationTurn)
{
    if (_buffSystem == nullptr)
        return;

    const bool shouldPlayBreakEffect =
        buffId == Buffs::Stun &&
        target.actor != nullptr &&
        target.statCom != nullptr &&
        !_buffSystem->Has_Buff(target, Buffs::Stun);

    _buffSystem->Apply_Buff(target, buffId, stack, durationTurn);

    if (shouldPlayBreakEffect)
    {
        PlayEffectDesc effectDesc{};
        effectDesc.effectName = Constants::Effect::BreakEffect;
        effectDesc.spawnBasisTarget = target.actor;
        effectDesc.localOffset = Vec3(0.f, 1.7f, 1.5f);
        effectDesc.scale = Vec3(1.f, 1.f, 1.f);
        CLIENT->Play_Effect(effectDesc);
    }
}

bool BattleController::Has_Buff(const BattleChar& target, Buffs buffId) const
{
    return _buffSystem != nullptr && _buffSystem->Has_Buff(target, buffId);
}

bool BattleController::Try_ApplySkillBuff(const Skill_Data& skill, const BattleChar& target)
{
    if (_buffSystem == nullptr || target.statCom == nullptr)
        return false;

    if (skill.buff == Buffs::None || skill.buffDurationTurn <= 0)
        return false;

    if (skill.ailmentChance <= 0.f)
        return false;

    if (skill.ailmentChance < 1.f && GAME->Random(0.f, 1.f) > skill.ailmentChance)
        return false;

    Apply_Buff(target, skill.buff, 1, skill.buffDurationTurn);

    if (skill.buff == Buffs::Stun && target.ModelCom)
    {
        target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Stun_Enter), 0.2f, false);
    }

    return true;
}

void BattleController::Complete_TurnStartStatusAnnounce()
{
    if (_buffSystem == nullptr)
        return;

    const BattleChar actor = _pendingTurnStartStatusActor;
    const Buffs pendingBuff = _pendingTurnStartStatusBuff;

    _pendingTurnStartStatusActor = {};
    _pendingTurnStartStatusBuff = Buffs::None;

    if (pendingBuff == Buffs::Burn)
    {
        _processedTurnStartStatuses.push_back(Buffs::Burn);

        if (actor.statCom && actor.statCom->Get_AliveStatus() == EAliveStatus::Dead)
        {
            _processedTurnStartStatuses.clear();
            _isWaitingTurnStartStatusHit = false;

            EndTurn();
            return;
        }

        _isWaitingTurnStartStatusHit = true;
        return;
    }

    if (pendingBuff == Buffs::Stun)
    {
        _processedTurnStartStatuses.push_back(Buffs::Stun);

        if (_buffSystem->Try_ConsumeStunOnTurnStart(actor))
            BattleAnimUtils::Play_BattleIdle(actor, 0.2f);

        _processedTurnStartStatuses.clear();
        _isWaitingTurnStartStatusHit = false;

        EndTurn();
        return;
    }
}

void BattleController::Play_BattleHitStop(float worldTimeScale, float duration)
{
    if (duration <= 0.f)
        return;

    GAME->Slow_WorldTime(worldTimeScale, duration);
}

void BattleController::Apply_ReactionApReward(const BattleReactionInputResult& result)
{
    if (!result.success ||
        result.apRewardAmount == 0 ||
        result.apRewardTargets.empty())
    {
        return;
    }

    for (const BattleChar& rewardTarget : result.apRewardTargets)
    {
        if (rewardTarget.actor == nullptr || rewardTarget.statCom == nullptr)
            continue;

        const uint32 beforeAp = rewardTarget.statCom->Get_CurAP();

        rewardTarget.statCom->Add_AP(result.apRewardAmount);

        const uint32 afterAp = rewardTarget.statCom->Get_CurAP();
        const uint32 gainedAp = afterAp - beforeAp;

        if (gainedAp > 0)
            OnBattleApChanged.Broadcast(rewardTarget, gainedAp, result.apRewardReason);
    }
}

void BattleController::Handle_ReactionSucceededForResult(BattleReactionType reactionType,
    const vector<BattleChar>& targets)
{
    if (_battleResultRecoder)
        _battleResultRecoder->Record_ReactionSucceeded(reactionType);
}

HRESULT BattleController::Ready_BattleTurnCamera()
{
    if (_battleTurnCamera != nullptr &&
        (_battleTurnCamera->Is_MarkDestroy() || _battleTurnCamera->Get_Transform() == nullptr))
    {
        _battleTurnCamera.reset();
    }

    if (_battleTurnCamera == nullptr)
    {
        // 카메라 재사용 X
        auto camera = GAME->Get_CameraByType(ObjectType::BattleTurnCamera);

        if (camera != nullptr && (camera->Is_MarkDestroy() || camera->Get_Transform() == nullptr))
            camera.reset();

        if (camera)
            _battleTurnCamera = dynamic_pointer_cast<BattleTurnCamera>(camera);
    }

    if (_battleTurnCamera == nullptr)
    {
        BattleTurnCamera::BATTLE_TURN_CAMERA_DESC desc{};
        desc.eye = XMFLOAT4(0.f, 8.f, -15.f, 1.f);
        desc.at = XMFLOAT4(0.f, 1.2f, 0.f, 1.f);
        desc.fovy = XMConvertToRadians(45.f);
        desc.nearPlane = 0.1f;
        desc.farPlane = 500.f;
        desc.mouseSensor = 0.15f;
        desc.mouseSensitivity = 0.f;

        Shared<GameObject> cameraObject;
        CHECK_FAILED(
            GAME->Add_GameObject(
                ETOI(LevelType::Static), TEXT("GameObject_BattleTurnCamera"),
                GAME->Current_LevelIndex(), TEXT("Layer_Camera"), cameraObject, &desc),
            E_FAIL
        );

        _battleTurnCamera = dynamic_pointer_cast<BattleTurnCamera>(cameraObject);
        CHECK_NULL(_battleTurnCamera, E_FAIL);

        _battleTurnCamera->Try_BeginPlay();
    }
    _battleTurnCamera->Reset_RuntimeState();
    _battleTurnCamera->Set_BattlePlaneYaw(_pendingBattlePlaneYaw);
    _battleTurnCamera->Set_Participants(_heroes, _enemies, _pendingBattleMonsterSpawnDescs);

    return S_OK;
}

void BattleController::Activate_BattleTurnCamera()
{
    auto camera = GAME->Get_CameraByType(ObjectType::BattleTurnCamera);

    if (camera)
        _battleTurnCamera = dynamic_pointer_cast<BattleTurnCamera>(camera);

    if (_battleTurnCamera == nullptr)
        return;

    BattleTurnCamera::BattleShotRequest request;
    request.mode = BattleCameraMode::BATTLEINTRO;
    request.subject = _enemies[_enemies.size() / 2].actor;
    request.useClip = true;
    string battleIntroCameraId = _enemies[_enemies.size() / 2].actor->Get_SourcePrefabName();
    if (battleIntroCameraId == Constants::Prefab::MB_Simon_V2)
        battleIntroCameraId = Constants::Prefab::MB_Simon;

    request.presetId = battleIntroCameraId;
    request.clipId = battleIntroCameraId;
    _battleTurnCamera->Apply_ShotRequest(request);

    const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();

    CLIENT->Request_BattleTurn();
}

// DOF_MODIFY 
void BattleController::Begin_BattleDOFOverride()
{
    if (FAILED(GAME->Begin_DOFOverrideScope(kBattleDOFOwnerTag)))
        LOG_WARN("BattleController: failed to begin battle DOF override scope");
}

void BattleController::End_BattleDOFOverride()
{
    GAME->End_DOFOverrideScope(kBattleDOFOwnerTag, Build_BattleDOFRestoreTransition());
}

Engine::DOFTransitionDesc BattleController::Build_BattleDOFRestoreTransition()
{
    Engine::DOFTransitionDesc transition{};
    transition.easing = Engine::DOFTransitionEasing::EaseInOut;
    transition.duration = 0.35f;
    return transition;
}

void BattleController::Restore_WorldCamera()
{
    CLIENT->Restore_PreviousCamera();
    GAME->Set_PlayerInputBlocked(false);
}

void BattleController::Update_DelayedEchoAttackSteps(float timeDelta)
{
    for (size_t i = 0; i < _delayedEchoAttackSteps.size();)
    {
        DelayedEchoAttackStep& step = _delayedEchoAttackSteps[i];
        step.remainingSec -= max(0.f, timeDelta);

        if (step.remainingSec > 0.f)
        {
            ++i;
            continue;
        }

        Apply_DelayedEchoAttackStep(step);
        _delayedEchoAttackSteps.erase(_delayedEchoAttackSteps.begin() + i);
    }
}

void BattleController::Apply_DelayedEchoAttackStep(const DelayedEchoAttackStep& step)
{
    if (!_isBattleActive || _battleEnded)
        return;

    if (step.attacker.actor == nullptr || step.attacker.statCom == nullptr || step.targets.empty())
        return;

    const bool guarded =
        _reactionController.TryConsumeGuard(
            step.reactionTarget,
            step.sourceStepIndex,
            step.consumeKey);

    if (guarded)
        return;

    // Echo 추가타는 BattleAction step/SkillScript CurIndex를 소비하지 않는 복제 데미지다.
    const float damage =
        DamageSystem::CalculatePlayerAttackDamage(
            step.attacker,
            step.powerMultiplier);

    DamageSystem::ApplyDamage(
        damage,
        step.attacker,
        step.targets,
        step.attackType);
}

void BattleController::Clear_DelayedEchoAttackSteps()
{
    _delayedEchoAttackSteps.clear();
    ANS_WeaponEcho::Clear_ActiveEchoWindows();
}

void BattleController::Clear_BattleRuntimeState()
{
    // 전투 종료/ F4로 게임 Stop할 때

    for (BattleChar& hero : _heroes)
    {
        if (hero.statCom)
            hero.statCom->Set_ReviveBlocked(false);
    }

    for (BattleChar& enemy : _enemies)
    {
        if (enemy.statCom)
            enemy.statCom->Set_ReviveBlocked(false);
    }

    Clear_DelayedEchoAttackSteps();

    if (_battleTutorialController)
        _battleTutorialController->Reset_RuntimeState();

    _turnAction = {};
    _turnActionRequest = {};
    _currentTurnActor = {};
    _pendingNextTurn = false;
    _requestEndTurnOnCurrentActorDeath = false;
    _reactionController.Clear_RuntimeState();
    _battleSpawnRotationReturns.clear();

    if (_qteController)
        _qteController->Clear();

    _hasPendingTutorialEnemyTurn = false;
    _isFreeAimShotPlaying = false;
    _freeAimShotAction = {};
    Clear_FreeAimHitTarget();
    currentTurnIndex = 0;
    _battleTime = 0.f;
    _nextTurnEntrySeq = 0;
    _flag = false;
    _isActionExecuting = false;
    _hitSequenceRunner.Clear();

    if (_pictosBattleSystem)
        _pictosBattleSystem->Clear_RuntimeState();

    _pendingTurnStartStatusBuff = Buffs::None;
    _pendingTurnStartStatusActor = {};

    _processedTurnStartStatuses.clear();
    _isWaitingTurnStartStatusHit = false;

    while (!_turnQueue.empty())
    {
        _turnQueue.pop();
    }

    if (_battleActionExe)
        _battleActionExe->Reset();

    if (_battleTurnCamera)
        _battleTurnCamera->Reset_RuntimeState();
}

void BattleController::Complete_BattleField()
{
    if (_isBattleActive)
    {
        Clear_BattleEnterPending();
        return;
    }

    _reactionController.Clear_RuntimeState();

    _isBattleActive = true;
    _isBattleEntering = false;
    _isWaitingBattleTransition = false;
    _battleEnded = false;
    _pendingNextTurn = false;

    const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    LOG_INFO(
        "[BattleCamTrace] BattleController::Complete_BattleField begin activeCameraType={} pendingHeroes={} pendingMonsterSpawns={} worldScale={:.3f}",
        static_cast<int>(activeCamera ? activeCamera->Get_ObjectType() : ObjectType::None),
        _pendingBattleHeroIds.size(),
        _pendingBattleMonsterSpawnDescs.size(),
        GAME->Get_WorldTimeScale());

    //// 카메라 저장
    //_worldCamera = GAME->Get_ActiveCamera();
    const uint32 battleFieldId = _pendingBattleFieldID;
    _activeBattleFieldID = battleFieldId;

    const bool isPlayerAdvantage = _pendingBattleIsPlayerAdvantage;

    // 현제 케릭 아이디 , 적 아이디로 전투 용 객체 생성
    Create_BattleChar(_pendingBattleHeroIds, _pendingBattleMonsterSpawnDescs);
    Capture_PreBattleBGM();

    if (_battleTutorialController)
        _battleTutorialController->Reset_ForBattle(battleFieldId, _battleSpawnRegistry);

    if (_battleResultRecoder)
        _battleResultRecoder->Reset_ForBattle();

    bool hasBossEnemy = false;
    for (const BattleChar& enemy : _enemies)
    {
        const Shared<MonsterStatCom> monsterStat = dynamic_pointer_cast<MonsterStatCom>(enemy.statCom);
        if (monsterStat && monsterStat->Is_Boss())
        {
            hasBossEnemy = true;
            break;
        }
    }

    const bool isBishopCinematicBGM =
        GAME->Get_CurrentBGM() == Constants::SoundBGM::BishopBattleBossMain;

    if (!isBishopCinematicBGM)
    {
        GAME->Play_BGM(
            hasBossEnemy ? Constants::SoundBGM::BattleBossBGM : Constants::SoundBGM::BattleNormalBGM,
            1.f,
            true,
            0.f);
    }

    // 초기 플레이 순서 지정
    Build_InitialTurnQueue(_pendingBattleIsPlayerAdvantage);

    // 들어온 캐릭터들의 위치 셋팅
    // FieldID로 
    SetCharInitPos();

    // 첫 입장시 플레이어 시작 에니 재생
    PlayerStartAnim();

    if (FAILED(Ready_BattleTurnCamera()))
    {
        LOG_ERROR("Create Failed : Battle Camera");

        Restore_WorldCamera();
        _isBattleActive = false;
        _battleEnded = true;

        if (_battleSpawnRegistry)
        {
            _battleSpawnRegistry->Destroy_SpawnedObjects();
            _battleSpawnRegistry->Clear_SourceWorldObject();
        }

        Clear_BattleRuntimeState();

        _heroes.clear();
        _enemies.clear();
        Clear_BattleEnterPending();
        _activeBattleFieldID = 0;
        return;
    }

    Begin_BattleDOFOverride();
    Activate_BattleTurnCamera();
    Clear_BattleEnterPending();

    // 턴 시작
    // battle start event만 넘겨주고, 전환 연출은 GameplayUI_Battle이 처리한다.
    OnBattleStarted.Broadcast(battleFieldId, isPlayerAdvantage);
    _isWaitingBattleTransition = true;
}

void BattleController::Clear_BattleEnterPending()
{
    _isBattleEntering = false;
    _hasRequestedBattleEnterCamera = false;
    _battleEnterSlow.elapsed = 0.f;

    _pendingBattleFieldID = 0;
    _pendingBattleIsPlayerAdvantage = false;
    _pendingBattleHeroIds.clear();
    _pendingBattleMonsterSpawnDescs.clear();
    _pendingBattleStartSpawnerId = 0;
    _pendingBattleStartOffset = Vec3::Zero;
    _pendingBattlePlaneYaw = 0.f;
}

bool BattleController::Try_ApplyFreeAimWeakPointBonus(const BattleFreeAimHitTarget& hitTarget,
    const BattleChar& attacker)
{
    if (!hitTarget.Is_Valid() || hitTarget.hitPart != FreeAimHitPart::WeakPoint)
        return false;

    const Shared<Monster_BattleBase> monster = dynamic_pointer_cast<Monster_BattleBase>(hitTarget.target.actor);
    if (monster == nullptr)
        return false;

    if (hitTarget.target.statCom != nullptr && hitTarget.target.statCom->Get_CurrentShield() > 0)
        return false;

    if (!monster->Try_Consume_FreeAimWeakPoint())
        return false; // 이미 소비됐으면 추가딜 없이 본체 피격만 남긴다.

    Play_BattleHitStop(0.08f, 0.10f); // 약간 슬로우

    // 일단은 고정 추가 데미지
    float freeAimWeakPointDamage = 11.f;

    // 튜토리얼은 즉사
    if (_activeBattleFieldID == BattleEncounterIds::LancelierTutorial &&
        dynamic_pointer_cast<MB_Lancelier>(monster) != nullptr &&
        hitTarget.target.statCom != nullptr)
    {
        freeAimWeakPointDamage = hitTarget.target.statCom->Get_RuntimeStat().CurHp;
    }

    DamageSystem::ApplyDamage(
        freeAimWeakPointDamage,
        attacker,
        hitTarget.target,
        AttackType::FreeAimWeak);

    return true;
}

void BattleController::Capture_PreBattleBGM()
{
    if (_hasCapturedPreBattleBGM)
        return;

    _preBattleBGM = GAME->Get_CurrentBGM();
    _hasCapturedPreBattleBGM = true;
}

void BattleController::Restore_PreBattleBGM()
{
    if (!_hasCapturedPreBattleBGM)
        return;

    if (_preBattleBGM.empty())
        GAME->Stop_SoundChannel(SoundChannel::BGM, 0.f);
    else
        GAME->Play_BGM(_preBattleBGM, 1.f, true, 0.f);

    _preBattleBGM.clear();
    _hasCapturedPreBattleBGM = false;
}

bool BattleController::Update_BattleTutorialGate(float timeDelta)
{
    if (_battleTutorialController)
        _battleTutorialController->Update(timeDelta);

    if (_battleTurnCamera)
        _battleTurnCamera->Set_RuntimePausedByTutorial(
            _battleTutorialController != nullptr &&
            _battleTutorialController->Should_PauseBattlePresentation());

    if (_battleTutorialController)
    {
        BattleReactionInputResult tutorialReactionResult{};

        if (_battleTutorialController->Try_HandleReactionInput(
            _reactionController, _turnAction, tutorialReactionResult))
        {
            Apply_ReactionApReward(tutorialReactionResult);
            return true; // 튜토리얼 리액션 입력이 이번 프레임 전투 입력을 소비
        }
    }

    if (_hasPendingTutorialEnemyTurn)
    {
        if (!_battleTutorialController || !_battleTutorialController->Should_BlockBattleUpdate())
        {
            _hasPendingTutorialEnemyTurn = false;
            Run_EnemyTurn(); // EnemyTurn 튜토리얼 확인 후에만 실제 적 행동 시작
        }

        return true;
    }

    if (_battleTutorialController && _battleTutorialController->Should_BlockBattleUpdate())
        return true; // 튜토리얼 UI가 열린 동안 전투 진행을 멈춘다.

    if (_battleTutorialController &&
        _currentTurnActor.statCom &&
        _currentTurnActor.statCom->Is_Player())
    {
        if (_battleTutorialController->Should_BlockPlayerBattleInput(
            Get_CurrentTurnState(),
            _turnActionRequest))
        {
            return true;
        }
    }

    return false;
}

void BattleController::Configure_BattlePlayerSkills(HeroId heroId, const Shared<CombatStatCom>& battleStat,
    const Shared<SkillTreeCom>& worldSkillTreeCom, const vector<string>& equippedSkillKeys)
{
    if (battleStat == nullptr)
        return;

    vector<string> battleSkillKeys = equippedSkillKeys;

    if (battleSkillKeys.empty() && worldSkillTreeCom)
        battleSkillKeys = worldSkillTreeCom->Get_UnlockedSkillIds(); // 장착값이 아직 없으면 해금 순서를 초기 5칸으로 사용한다.

    battleSkillKeys.erase(
        remove_if(battleSkillKeys.begin(), battleSkillKeys.end(),
            [heroId](const string& skillKey)
            {
                return GradientSkill::Is_GradientSkill(heroId, skillKey);
            }), battleSkillKeys.end());

    // 5칸만 세팅
    battleSkillKeys.resize(GradientSkill::NormalSkillSlotCount);

    const char* gradientSkillKey = GradientSkill::Resolve_SkillKey(heroId);
    if (gradientSkillKey[0] != '\0' &&
        worldSkillTreeCom != nullptr &&
        worldSkillTreeCom->IsUnlocked(gradientSkillKey) &&
        CLIENT->Get_Skill(gradientSkillKey) != nullptr)
    {
        battleSkillKeys.push_back(gradientSkillKey); // 6번째 슬롯은 BattleCommand Q 전용 Gradient로 소비한다.
    }
    else
    {
        battleSkillKeys.push_back(""); // 잠긴 Gradient도 slot index를 유지한다.
    }

    for (string& skillKey : battleSkillKeys)
    {
        if (skillKey.empty())
            continue;

        const Skill_Data* skillData = CLIENT->Get_Skill(skillKey);
        if (skillData == nullptr)
        {
            LOG_WARN("Battle skill data missing. heroId={}, skillKey={}",
                magic_enum::enum_name(heroId), skillKey.c_str());
            skillKey.clear(); // 슬롯 순서는 유지하고, 해당 칸만 비운다.
            continue;
        }

        if (worldSkillTreeCom && !worldSkillTreeCom->IsUnlocked(skillKey))
        {
            LOG_WARN("Battle skill locked. heroId={}, skillKey={}",
                magic_enum::enum_name(heroId), skillKey.c_str());
            skillKey.clear(); // 잠긴 스킬도 다른 슬롯을 당기지 않는다.
            continue;
        }
    }

    battleStat->Set_SkillKeys(battleSkillKeys);
}

PlayerCharacterType BattleController::To_PlayerCharacterType(HeroId heroId)
{
    switch (heroId)
    {
    case HeroId::Gustave:
        return PlayerCharacterType::Gustave;
    case HeroId::Lune:
        return PlayerCharacterType::Lune;
    case HeroId::Maelle:
        return PlayerCharacterType::Maelle;
    case HeroId::END:
    default:
        return PlayerCharacterType::None;
    }
}

bool BattleController::Try_BeginTurnStartStatusAnnounce()
{
    CHECK_NULL(_buffSystem, false);

    const auto isProcessed = [this](Buffs buffId)
        {
            return ::find(
                _processedTurnStartStatuses.begin(),
                _processedTurnStartStatuses.end(),
                buffId) != _processedTurnStartStatuses.end();
        };

    if (!isProcessed(Buffs::Burn) &&
        _buffSystem->Has_TurnStartBurn(_currentTurnActor))
    {
        _pendingTurnStartStatusBuff = Buffs::Burn;
        _pendingTurnStartStatusActor = _currentTurnActor;
        OnTurnStartStatusAnnounceRequested.Broadcast(_currentTurnActor, Buffs::Burn);
        _buffSystem->Apply_BurnTurnStartDamage(_currentTurnActor);
        return true;
    }

    if (!isProcessed(Buffs::Stun) &&
        _buffSystem->Has_Buff(_currentTurnActor, Buffs::Stun))
    {
        _pendingTurnStartStatusBuff = Buffs::Stun;
        _pendingTurnStartStatusActor = _currentTurnActor;
        OnTurnStartStatusAnnounceRequested.Broadcast(_currentTurnActor, Buffs::Stun);
        return true;
    }

    return false;
}

void BattleController::Continue_StartTurnAfterStatusEffects()
{
    const auto actor = _currentTurnActor;

    if (actor.statCom->Is_Player())
    {
#ifdef _DEBUG
        wstring skillDebugText = L"Player Turn Skills: ";

        const vector<string>& skillKeys = actor.statCom->Get_SkillKeys();
        for (size_t i = 0; i < 5; ++i)
        {
            if (i > 0)
                skillDebugText += L" / ";

            skillDebugText += to_wstring(i + 1);
            skillDebugText += L":";

            if (i < skillKeys.size() && !skillKeys[i].empty())
                skillDebugText += wstring(skillKeys[i].begin(), skillKeys[i].end());
            else
                skillDebugText += L"-";
        }

        GAME->Print(L"{}", skillDebugText, duration(0.f));
#endif

        uint32 turnStartApGain = 1u; // 기본 턴 시작 AP 회복량

        if (_pictosBattleSystem)
        {
            _pictosBattleSystem->ProcessTurnStart(actor); // 예열, 룰렛 처리
            turnStartApGain += _pictosBattleSystem->Resolve_TurnStartApBonus(actor); // 활력의 턴 + 1
        }

        const uint32 beforeAp = actor.statCom->Get_CurAP();

        actor.statCom->Add_AP(turnStartApGain);

        const uint32 afterAp = actor.statCom->Get_CurAP();
        const uint32 gainedAp = afterAp - beforeAp;

        if (gainedAp > 0)
        {
            // 최대 AP라 실제 증가가 없으면 안띄우기
            OnBattleApChanged.Broadcast(actor, gainedAp, EAPChangeReason::TurnStart);
        }

        string gradientSkillKey{};
        if (actor.statCom->Try_GetSkillKeyBySlot(GradientSkill::SlotIndex, gradientSkillKey))
        {
            actor.statCom->Add_GradientGauge(GradientSkill::GaugeGainPerTurn);
        }

        _battleFSM->Change_State(BattleTurnState::ChooseAction);

        if (_currentTurnActor.ModelCom)
        {
            const bool playedTurnStart =
                _currentTurnActor.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Turn_Start),
                    0.2f,
                    false
                ); // 현재 턴 캐릭터만 턴 시작 애니메이션을 시작

            if (!playedTurnStart)
                BattleAnimUtils::Play_BattleIdle(_currentTurnActor, 0.2f); // Turn_Start 매핑이 없으면 Base Idle
        }

        for (const BattleChar& hero : _heroes)
        {
            if (hero.actor == nullptr || hero.actor == _currentTurnActor.actor)
                continue;

            if (hero.ModelCom == nullptr || hero.statCom == nullptr)
                continue;

            if (hero.statCom->Get_AliveStatus() == EAliveStatus::Dead)
                continue;

            BattleAnimUtils::Play_BattleIdle(hero, 0.2f);
        }

        EvtBattlePresentation event{};
        event.cameraMode = BattleCameraMode::PLAYERFOCUS;
        event.subject = _currentTurnActor.actor;
        LOG_INFO("[BattleCamTrace] Publish PLAYERFOCUS from BattleController::Continue_StartTurnAfterStatusEffects");
        GAME->Get_EventBus().Publish(event);

        OnTurnStarted.Broadcast(_currentTurnActor, BattleTurnState::ChooseAction);
        return;
    }

    OnTurnStarted.Broadcast(_currentTurnActor, BattleTurnState::None);

    if (_battleTutorialController && _battleTutorialController->Should_DeferEnemyTurnStart())
    {
        Request_WidePresentationMode(); // Gradient 튜토리얼로 적 행동을 미뤄도 구조물에 카메라가 붙지 않게 기본 전투 구도로 고정한다.

        _hasPendingTutorialEnemyTurn = true;
        return;
    }

    Run_EnemyTurn();
}

void BattleController::Set_BattleFSM()
{
    _battleFSM = BattleStateMachine::Create();

    _battleFSM->Add_State(BattleTurnState::ChooseSkill, ChooseSkill::Create(GetSharedPtr<BattleController>()));
    _battleFSM->Add_State(BattleTurnState::ChooseGradient, ChooseGradient::Create(GetSharedPtr<BattleController>()));
    _battleFSM->Add_State(BattleTurnState::AttackTargeting, AttackTargeting::Create(GetSharedPtr<BattleController>()));
    _battleFSM->Add_State(BattleTurnState::AttackPlaying, AttackPlaying::Create(GetSharedPtr<BattleController>()));
    _battleFSM->Add_State(BattleTurnState::ChooseItem, ChooseItem::Create(GetSharedPtr<BattleController>()));
    _battleFSM->Add_State(BattleTurnState::ChooseAction, ChooseAction::Create(GetSharedPtr<BattleController>()));
    _battleFSM->Add_State(BattleTurnState::SkillTargeting, SkillTargeting::Create(GetSharedPtr<BattleController>()));
    _battleFSM->Add_State(BattleTurnState::ItemTargeting, ItemTargeting::Create(GetSharedPtr<BattleController>()));
    _battleFSM->Add_State(BattleTurnState::UltimateTargeting, UltimateTargeting::Create(GetSharedPtr<BattleController>()));
    _battleFSM->Add_State(BattleTurnState::ChooseUltimate, ChooseUltimate::Create(GetSharedPtr<BattleController>()));
    _battleFSM->Add_State(BattleTurnState::FreeAimTargeting, FreeAimTargeting::Create(GetSharedPtr<BattleController>()));

    //_battleFSM->Change_State(BattleTurnState::ChooseAction);
}

bool BattleController::Is_BattleFieldActiveOrEntering(uint32 fieldId) const
{
    return fieldId != 0 &&
        ((_isBattleEntering && _pendingBattleFieldID == fieldId) ||
            (_isBattleActive && _activeBattleFieldID == fieldId));
}

void BattleController::End_FirstAnim()
{
    if (_firstAnimflag)
        return;

    ++_firstAnimCount;

    if (_firstAnimCount >= static_cast<int>(_firstAnimTargetCount))
    {
        for (const auto it : _heroes)
        {
            if (it.ModelCom)
                it.ModelCom->Set_RootMotionEnabled(true);
        }

        _firstAnimflag = true;
    }
}

void BattleController::PlayerStartAnim()
{
    _firstAnimflag = false;
    _firstAnimCount = 0;
    _firstAnimTargetCount = _battleSpawnRegistry ? _battleSpawnRegistry->Get_BattleHeroSpawnCount() : 0;

    for (const auto it : _heroes)
    {
        if (it.ModelCom == nullptr)
            continue;

        it.ModelCom->Set_RootMotionEnabled(false);
        it.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Battle_Start), false);
    }

    if (_firstAnimTargetCount == 0)
    {
        _firstAnimflag = true;
    }

    for (const auto it : _enemies)
    {
        if (it.ModelCom)
            it.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Base_Idle), 0.3f, true);
    }
}

void BattleController::HitAction(BattleChar target, AttackType attackType, bool isCritical)
{
    if (target.ModelCom == nullptr || target.statCom == nullptr)
    {
        LOG_WARN("HitAction: missing components. model={}, stat={}",
            target.ModelCom != nullptr,
            target.statCom != nullptr);
        return;
    }

    if (target.statCom->Get_AliveStatus() == EAliveStatus::Dead)
    {
        const bool playedDead = target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Dead), 0.3f, false);
        LOG_DEBUG("HitAction: dead anim result={}, attackType={}, actor={}",
            playedDead,
            static_cast<int>(attackType),
            target.actor ? String::ToString(target.actor->Get_Name()).c_str() : "null");
        return;
    }

    const bool isStunned = _buffSystem && _buffSystem->Has_Buff(target, Buffs::Stun);
    if (isStunned) // 스턴이면 Hit Stun
    {
        target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Hit_Stun), 0.2f, false);
        return;
    }

    // 크리티컬이면 Heavy
    if (isCritical)
    {
        target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Hit_Heavy), 0.3f, false);
        return;
    }

    switch (attackType)
    {
    case AttackType::BuffEffect:
    case AttackType::FreeAimLight:
    case AttackType::Combo:
        target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Hit_Light), 0.3f, false);
        break;

    case AttackType::BaseAttack:
    case AttackType::SingleSkill:
        target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Hit_Medium), 0.3f, false);
        break;

    case AttackType::SingleAOE:

    case AttackType::ComboLast:
        target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Hit_Heavy), 0.3f, false);
        break;
    case AttackType::FreeAimWeak:
        if (false == target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Hit_Weak), 0.3f, false))
            target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Hit_Heavy), 0.3f, false);
        break;
    case AttackType::Counter:
        break;
    }
}

Shared<BattleController> BattleController::Create()
{
    Shared<BattleController> instance = make_shared<BattleController>();

    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Created : BattleController");
        return nullptr;
    }

    if (FAILED(instance->Ready_Delegates()))
    {
        instance->Free();
        return nullptr;
    }

    return instance;
}

void BattleController::Free()
{
    if (_editorStopHandle.IsValid())
    {
        GAME->Get_DelegateHub().OnEditorStopRuntime.Remove(_editorStopHandle);
        _editorStopHandle.Reset();
    }

    if (_damageAppliedHandle.IsValid())
    {
        DamageSystem::OnDamageApplied.Remove(_damageAppliedHandle);
        _damageAppliedHandle.Reset();
    }

    if (_reactionSucceededResultHandle.IsValid())
        _reactionController.OnReactionSucceeded.Remove(_reactionSucceededResultHandle);

    _reactionSucceededResultHandle.Reset();

    auto ReleaseCtrl = [](auto& ctrl)
        {
            if (ctrl)
            {
                ctrl->Free();
                ctrl.reset();
            }
        };

    ReleaseCtrl(_battleSpawnRegistry);
    ReleaseCtrl(_qteController);
    ReleaseCtrl(_buffSystem);
    ReleaseCtrl(_battleTutorialController);
    ReleaseCtrl(_battleResultRecoder);
    ReleaseCtrl(_pictosBattleSystem);

    Base::Free();
}
