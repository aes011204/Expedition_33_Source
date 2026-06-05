#include "BattleController.h"

#include "AttackPlaying.h"
#include "BuffSystem.h"
#include "Character.h"
#include "ClientInstance.h"
#include "GameDataTable.h"
#include "AttackTargeting.h"
#include "BattleStateMachine.h"
#include "ChooseAction.h"
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
#include "BattleStartSpawner.h"
#include "EffectCom.h"
#include "Monster_WorldBase.h"
#include "ScreenFx_Manager.h"

REGISTER_GLOBAL_ENUM(BattleAnimType, "BattleAnimType")

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

    //    _targetPreviewChangedHandle = OnTargetPreviewChanged.Add(this,&BattleController::Handle_TargetPreviewChanged);

    return S_OK;
}

void BattleController::Update(float timeDelta)
{
    if (_isBattleEntering)
    {
        if (!_hasRequestedBattleEnterCamera)
        {
            const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
            LOG_INFO(
                "[BattleCamTrace] BattleController::Update request BATTLE_ENTER activeCameraType={} elapsed={:.3f}/{:.3f} worldScale={:.3f}",
                static_cast<int>(activeCamera ? activeCamera->Get_ObjectType() : ObjectType::None),
                _battleEnterSlow.elapsed,
                _battleEnterSlow.duration,
                GAME->Get_WorldTimeScale());

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

    // const bool isBattleCameraLiveEditActive =
    //_battleTurnCamera != nullptr &&
    //_battleTurnCamera->Is_LiveEditEnabled();

    //if (isBattleCameraLiveEditActive)
    //return;

    //UpdateReactionInput();
    //if (_pendingNextTurn)
    _reactionController.UpdateAnimation(timeDelta);

    if (_isPendingVictorySlow)
    {
        _victorySlow.elapsed += GAME->Get_UnscaledTimeDelta();

        if (_victorySlow.elapsed >= _victorySlow.duration)
        {
            _isPendingVictorySlow = false;
            _victorySlow.elapsed = 0.f;

            OnExitBattleField(EBattleEndResult::Victory);
            _battleEnded = true;
            return;
        }
    }

    if (!_isPendingVictorySlow && _requestEndTurnOnCurrentActorDeath)
    {
        _requestEndTurnOnCurrentActorDeath = false;
        EndTurn();
        return;
    }

    if (!_isPendingVictorySlow)
    {
        // 패링, 회피 키인풋
        UpdateReactionInput();

        // 한턴 끝나면 여기로
        if (_pendingNextTurn)
        {
            _battleActionExe->Reset();
            StartTurn();
            _pendingNextTurn = false;
            return;
        }
    }

    if (_heroes.empty() == true || _enemies.empty() == true || _currentTurnActor.actor == nullptr)
        return;

    // 적턴 인지 플레이어 턴인지에 따라
    if (!_isPendingVictorySlow && _currentTurnActor.statCom->Is_Player())
        _battleFSM->Update_StateMachine(timeDelta);

    // Script 다단히트 적용
    _hitSequenceRunner.Update(timeDelta);

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

    if (_isActionExecuting &&
        _turnAction.Get_CurChar().actor != nullptr &&
        _battleActionExe->IsFinished())
    {
        if (_reactionController.IsBusy())
            return;

        _isActionExecuting = false;
        EndTurn();
        return;
    }
}

void BattleController::Run_EnemyTurn()
{
    //LOG_DEBUG(
    //    "Run_EnemyTurn: actor={}",
    //    Get_TurnActionRequest().actor.actor ? String::ToString(Get_TurnActionRequest().actor.actor->Get_Name()).c_str() : "null"
    //);

    const auto monster
        = dynamic_pointer_cast<Monster_BattleBase>(Get_TurnActionRequest().actor.actor);

    CHECK_NULL(monster);

    monster->Decide_Action_BTOnly(Get_TurnActionRequest(), Get_AliveHeroes());

    Play_EnemyTargetedStart(Get_TurnActionRequest().InitTarget);

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
    const Shared<GameObject>& sourceObject)
{
    if (_isBattleActive || _isBattleEntering)
        return;

    _isBattleEntering = true;
    _battleEnterSlow.elapsed = 0.f;
    _battleEnded = false;
    _pendingNextTurn = false;

    const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    LOG_INFO(
        "[BattleCamTrace] BattleController::OnEnterBattleField field={} heroes={} monsterSpawns={} activeCameraType={} worldScaleBefore={:.3f} slowScale={:.3f} slowDuration={:.3f}",
        FieldID,
        heroIds.size(),
        battleMonsterSpawnDescs.size(),
        static_cast<int>(activeCamera ? activeCamera->Get_ObjectType() : ObjectType::None),
        GAME->Get_WorldTimeScale(),
        _battleEnterSlow.worldTimeScale,
        _battleEnterSlow.duration);

    // 배틀 진입 소스 오브젝트 세팅
    // ex) MW_Lancelier랑 충돌돼서 전투 시작하면 저장하고있다가, 전투 종료 후 삭제하려고
    _battleSpawnRegistry->Set_SourceWorldObject(sourceObject, GAME->Current_LevelIndex());

    // 2초 뒤 실제 배틀 생성 데이터 추가
    _pendingBattleFieldID = FieldID;
    _pendingBattleIsPlayerAdvantage = isPlayerAdvantage;
    _pendingBattleHeroIds = heroIds;
    _pendingBattleMonsterSpawnDescs = battleMonsterSpawnDescs;
    _pendingBattleStartSpawnerId = 0;
    _pendingBattleStartOffset = Vec3::Zero;

    if (Shared<Monster_WorldBase> worldMonster = dynamic_pointer_cast<Monster_WorldBase>(sourceObject))
    {
        _pendingBattleStartSpawnerId = worldMonster->Get_BattleStartSpawnerId();
        _pendingBattleStartOffset = worldMonster->Get_BattleStartOffset();
    }

    // 월드 캐릭터 입력 막기
    GAME->Set_PlayerInputBlocked(true);
    //_worldCamera = GAME->Get_ActiveCamera();
    GAME->Slow_WorldTime(_battleEnterSlow.worldTimeScale, _battleEnterSlow.duration);
}

EBattleEndResult BattleController::OnExitBattleField(EBattleEndResult result)
{
    _lastVictoryRewardExp = 0;

    if (result == EBattleEndResult::Victory && _battleSpawnRegistry)
    {
        _lastVictoryRewardExp = _battleSpawnRegistry->Get_BattleRewardExp();
        Apply_VictoryRewardExp(_lastVictoryRewardExp);
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
        _isWaitingVictoryConfirm = true;
        _isBattleEntering = false;
        _isWaitingBattleTransition = false;

        GAME->Stop_WorldTimeSlow();
        GAME->Set_WorldTimeScale(0.0001f);
        GAME->Set_PlayerInputBlocked(true);

        return result;
    }

    _isBattleActive = false;
    _isBattleEntering = false;
    _isWaitingBattleTransition = false;

    // Clear_TargetPreview();
    // OnBattleEnded.Broadcast(result); // battle end UI 정리는 GameplayUI_Battle에서 처리한다.

    // // 전투 종료 시 슬로우모션 값이 남지 않도록 복구
    // GAME->Stop_WorldTimeSlow();

     //Restore_WorldCamera(); // 카메라 복구
    CLIENT->Restore_PreviousCamera();
    GAME->Set_PlayerInputBlocked(false);

    // 도망치기
    // 실패
    // 성공

    if (_battleSpawnRegistry)
    {
        _battleSpawnRegistry->Destroy_SpawnedObjects();
        _battleSpawnRegistry->Clear_SourceWorldObject();
    }

    Clear_BattleRuntimeState();

    _heroes.clear();
    _enemies.clear();

    Clear_BattleEnterPending();

    // 초기 애님 관련 변수 초기화
    _firstAnimflag = false;
    _firstAnimCount = 0;

    return result;
}

void BattleController::ExecuteAction(const BattleAction& turnAction)
{
    Clear_QTEState(); // 새 액션 시작 시 이전 QTE 결과가 다음 스킬에 섞이지 않게 초기화한다.

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
    Fail_QTEIfActive(); // 턴이 먼저 끝나면 열린 QTE는 실패로 확정한다.

    _isActionExecuting = false;

    // 이번 적 턴에서 공격하는 플레이어 Targeted End 애니메이션 재생
    Play_EnemyTargetedEnd();

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

    LOG_DEBUG(
        "EndTurn: actor={}",
        _currentTurnActor.actor ? String::ToString(_currentTurnActor.actor->Get_Name()).c_str() : "null"
    );
    LOG_DEBUG("EndTurn: deadPlayer={}, deadEnemy={}", DeadPlayer, DeadEnemy);

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
    _buffSystem->ProcessTurnEnd(_currentTurnActor.statCom);
    OnTurnEnded.Broadcast(_currentTurnActor); // 턴 종료 신호

    const auto actor = _currentTurnActor;
    // 죽은애들은 큐에 다시 안들어가게
    if (actor.statCom->Get_AliveStatus() != EAliveStatus::Dead)
    {
        TurnEntry entry{};
        entry.hero = actor;

        entry.nextTime =
            _battleTime + 100.f / max(actor.statCom->Get_Speed(), 1.f);

        _turnQueue.push(entry);
    }

    _pendingNextTurn = true;
}

void BattleController::StartTurn()
{
    ClearReactionState(); // 이전 턴에서 소비못한 리액션 상태 정리
    _isActionExecuting = false; // 선택 대기 상태에서는 executor 완료 여부로 턴을 넘기지 않는다.
    _requestEndTurnOnCurrentActorDeath = false;

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
    //_turnAction = {};
    //_turnAction._curChar = _currentTurnActor;
    _turnActionRequest = {};
    _turnActionRequest.actor = _currentTurnActor;
    const auto actor = _currentTurnActor;

    // 상태 이상 엔드
    _buffSystem->ProcessTurnStart(_currentTurnActor.statCom);

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

    // fsm
    if (actor.statCom->Is_Player())
    {
        const uint32 beforeAp = actor.statCom->Get_CurAP();

        actor.statCom->Add_AP(1);

        const uint32 afterAp = actor.statCom->Get_CurAP();
        const uint32 gainedAp = afterAp - beforeAp;

        if (gainedAp > 0)
        {
            // 최대 AP라 실제 증가가 없으면 안띄우기
            OnBattleApChanged.Broadcast(actor, gainedAp, EAPChangeReason::TurnStart);
        }

        _battleFSM->Change_State(BattleTurnState::ChooseAction);

        EvtBattlePresentation event;
        event.cameraMode = BattleCameraMode::PLAYERFOCUS;
        event.subject = _currentTurnActor.actor;
        GAME->Get_EventBus().Publish(event);

        OnTurnStarted.Broadcast(_currentTurnActor, BattleTurnState::ChooseAction);
    }
    else
    {
        OnTurnStarted.Broadcast(_currentTurnActor, BattleTurnState::None);
        Run_EnemyTurn();
    }
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
        _battleSpawnRegistry->Register_SpawnObject(character, GAME->Current_LevelIndex());

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
        const Shared<SkillTreeCom> worldSkillTreeCom = worldHero->Get_Component<SkillTreeCom>();

        if (worldStat && battleStat)
            battleStat->Apply_BattleSyncSnapshot(worldStat->Create_BattleSyncSnapshot());

        if (battleStat)
            battleStat->Set_BattleStartAp();

        // World Skill Tree 읽어서 세팅
        Configure_BattlePlayerSkills(heroId, battleChar.statCom, worldSkillTreeCom);

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

    while (!_turnQueue.empty())
    {
        _turnQueue.pop();
    }

    // 선제 타격이면 몬스털르 늦추고, Player <-> Monster 충돌 진입이면 플레이어를 낮추기
    if (isPlayerAdvantage)
        monsterOffset = 5.f;
    else
        heroOffset = 5.f;

    for (const auto& hero : _heroes)
    {
        if (!hero.actor)
            continue;

        TurnEntry entry = {};
        entry.nextTime = 100.f / max(hero.statCom->Get_Speed(), 1.f) + heroOffset;
        entry.hero = hero;
        _turnQueue.push(entry);
    }
    for (const auto& enemy : _enemies)
    {
        if (!enemy.actor)
            continue;

        TurnEntry entry = {};
        entry.nextTime = 100.f / max(enemy.statCom->Get_Speed(), 1.f) + monsterOffset;
        entry.hero = enemy;
        _turnQueue.push(entry);
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

    rebuiltQueue.push(revivedEntry);
    _turnQueue = std::move(rebuiltQueue);
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
        }
    }
}

void BattleController::Request_WidePresentationMode()
{
    if (!_battleTurnCamera)
        return;

    EvtBattlePresentation event;
    event.cameraMode = BattleCameraMode::WIDE;
    event.subject = nullptr;
    GAME->Get_EventBus().Publish(event);
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

void BattleController::Handle_EditorStopRuntime()
{
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
    Restore_WorldCamera();

    if (_battleFSM)
        _battleFSM->Change_State(BattleTurnState::ChooseAction);

    //
    _firstAnimflag = false;
    _firstAnimCount = 0;
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

    return S_OK;
}

void BattleController::OpenReactionWindow(ReactionType type)
{
    _reactionController.OpenWindow(type);
}

void BattleController::CloseReactionWindow(ReactionType type)
{
    _reactionController.CloseWindow(type);
}

void BattleController::ClearReactionState()
{
    _reactionController.Clear();
}

void BattleController::UpdateReactionInput()
{
    const ReactionResult result =
        _reactionController.UpdateInput(_turnAction);

    if (!result.success)
        return;

    // Reaction 성공 브로드캐스트
    if (result.apRewardAmount > 0 &&
        result.apRewardTarget.actor != nullptr &&
        result.apRewardTarget.statCom != nullptr)
    {
        const uint32 beforeAp = result.apRewardTarget.statCom->Get_CurAP();

        result.apRewardTarget.statCom->Add_AP(result.apRewardAmount);

        const uint32 afterAp = result.apRewardTarget.statCom->Get_CurAP();
        const uint32 gainedAp = afterAp - beforeAp;

        if (gainedAp > 0)
        {
            // Max치면 증가 X
            OnBattleApChanged.Broadcast(result.apRewardTarget, gainedAp, result.apRewardReason);
        }
    }

    OnReactionSucceeded.Broadcast(result.type, _turnAction.GetCurStepTarget());
}

bool BattleController::Try_ConsumeReactionGuard(const BattleChar& target)
{
    if (target.actor == nullptr || target.statCom == nullptr)
        return false;

    // Reaction은 몬스터 공격을 받는 플레이어한테만 적용
    if (!target.statCom->Is_Player())
        return false;

    ReactionType result = ReactionType::None;

    return _reactionController.TryConsume(result, _turnAction.GetCurStepIndex());
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

void BattleController::StartCounter(ModelCom* sourceModel)
{
    _reactionController.StartCounter(sourceModel);
}

void BattleController::ApplyCounterDamage(ModelCom* sourceModel)
{
    _reactionController.ApplyCounterDamage(sourceModel);
}

void BattleController::BeginCounterTimeSection(ModelCom* sourceModel, float worldTimeScale, float counterFrameSpeed)
{
    _reactionController.BeginCounterTimeSection(sourceModel, worldTimeScale, counterFrameSpeed);
}

void BattleController::EndCounterTimeSection(ModelCom* sourceModel)
{
    _reactionController.EndCounterTimeSection(sourceModel);
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

    /*LOG_DEBUG(
        "Execute_Skill: actor={}, target={}, script={}",
        actor.actor ? String::ToString(actor.actor->Get_Name()).c_str() : "null",
        action.GetCurStepTarget()[0].actor ? String::ToString(action.GetCurStepTarget()[0].actor->Get_Name()).c_str() : "null",
        action.GetSkillData().scriptClassName.c_str()
    );*/

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

    if (apCost > 0 && !actor.statCom->Try_ConsumeAP(apCost))
    {
        LOG_DEBUG("Execute_Skill: not enough AP. skill={}, curAP={}, cost={}",
            action.GetSkillData().skillKey.c_str(),
            actor.statCom->Get_CurAP(),
            apCost);

        Clear_CurrentScript();

        if (actor.statCom->Is_Player() && _battleFSM)
            _battleFSM->Change_State(BattleTurnState::ChooseSkill); // 플레이어는 다시 스킬을 고를 수 있게 되돌린다.
        else
            EndTurn();

        return;
    }

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
        if (action.GetInitTarget().statCom->Get_AliveStatus() != EAliveStatus::Alive)
            return; // 치유 틴트는 살아있는 대상에게만 적용한다.

        DamageSystem::Heal(action.GetInitTarget().statCom->Get_StatInfo().maxHp * static_cast<float>(itemData.healHp) * 0.01f, action.GetInitTarget());
        break;

    case BattleItemType::EnergyTint:
        if (action.GetInitTarget().statCom->Get_AliveStatus() != EAliveStatus::Alive)
            return; // 에너지 틴트는 살아있는 대상에게만 적용한다.

        action.GetInitTarget().statCom->Add_AP(itemData.restoreAp);
        break;

    case BattleItemType::ReviveTint:
        if (action.GetInitTarget().statCom->Get_AliveStatus() != EAliveStatus::Dead)
            return; // 부활 틴트는 죽어있는 대상에게만 적용한다.

        //action.GetInitTarget().statCom->Set_AliveStatus(EAliveStatus::Alive); // 먼저 부활 상태로 복구한다.
        //Restore_BattleTargetCollision(action.GetInitTarget()); // 부활 직후 다시 충돌 가능 상태로 전환

        //DamageSystem::Heal(action.GetInitTarget().statCom->Get_StatInfo().maxHp * static_cast<float>(itemData.reviveHp) * 0.01f, action.GetInitTarget());

        //if (action.GetInitTarget().ModelCom)
        //{
        //    if (!action.GetInitTarget().ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Revive), 0.2f, false))
        //    {
        //        BattleAnimUtils::Play_BattleIdle(
        //            action.GetInitTarget(),
        //            0.2f); // Revive 매핑이 없으면 부활된 대상 기준 idle로 복귀한다.
        //    }
        //}

        //Rebuild_TurnQueue(action.GetInitTarget());
        Revive_BattleMon(action.GetInitTarget(), static_cast<float>(itemData.reviveHp) * 0.01f);
        break;

    default:
        return;
    }

    CLIENT->Consume_Item(itemData.itemType);
    Clear_TargetPreview();
}


void BattleController::Revive_BattleMon(const BattleChar& battleChar, float MultipleHp)
{
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

void BattleController::Play_EnemyTargetedStart(const BattleChar& target)
{
    _enemyTurnTargetedActor = {};
    _hasEnemyTurnTargetedActor = false;
    if (target.actor == nullptr || target.ModelCom == nullptr || target.statCom == nullptr)
        return;

    if (!target.statCom->Is_Player())
        return;

    _enemyTurnTargetedActor = target;
    _hasEnemyTurnTargetedActor = true;

    // Start -> Idle은 Notify에서 제어
    if (!target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Targeted_Start), 0.2f, false))
    {
        // fallback
        if (!target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Targeted_Idle), 0.2f, true))
            BattleAnimUtils::Play_BattleIdle(target, 0.2f);
    }
}

void BattleController::Play_EnemyTargetedEnd()
{
    if (!_hasEnemyTurnTargetedActor)
        return;

    const BattleChar target = _enemyTurnTargetedActor;
    _enemyTurnTargetedActor = {};
    _hasEnemyTurnTargetedActor = false;

    if (target.actor == nullptr || target.ModelCom == nullptr)
        return;
    if (target.statCom->Get_AliveStatus() == EAliveStatus::Dead)
        return;
    if (!target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Targeted_End), 0.2f, false))
        BattleAnimUtils::Play_BattleIdle(target, 0.2f);
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

        TurnEntry turn = {};
        turn.nextTime = _battleTime + 100.f / max(battleChar.statCom->Get_Speed(), 1.f);

        turn.hero = battleChar;


        _turnQueue.push(turn);
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


        TurnEntry turn = {};
        turn.nextTime = _battleTime + 100.f / max(battleChar.statCom->Get_Speed(), 1.f);
        turn.hero = battleChar;


        _turnQueue.push(turn);
    }
}

vector<BattleChar> BattleController::Get_TurnOrderPreview(uint32 maxCount) const
{
    vector<BattleChar> preview{};
    if (maxCount == 0)
        return preview;

    auto queueCopy = _turnQueue;

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
        queueCopy.push(entry);
    }

    return preview;
}

void BattleController::Confirm_Victory()
{
    if (!_isWaitingVictoryConfirm)
        return;

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

    Restore_WorldCamera();

    Clear_BattleRuntimeState();

    // 배틀 플레이어, 몬스터 삭제
    if (_battleSpawnRegistry)
    {
        _battleSpawnRegistry->Destroy_SpawnedObjects();
        _battleSpawnRegistry->Destroy_SourceWorldObject();
    }

    _heroes.clear();
    _enemies.clear();

    Clear_BattleEnterPending();

    // 초기 애님 관련 변수 초기화
    _firstAnimflag = false;
    _firstAnimCount = 0;
}

void BattleController::Handle_DamageApplied(const DamageAppliedInfo& info)
{
    if (info.damage > 0.f && info.target.actor)
    {
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

void BattleController::Begin_QTE(const BattleQTERequest& request)
{
    CHECK_NULL(_qteController);

    if (!_isBattleActive || _battleEnded)
        return;

    _qteController->Begin(request);

    // QTE UI는 GameplayUI_Battle이 소유하므로 시작 신호만 보낸다.
    OnQTEStarted.Broadcast(request);
}

void BattleController::Resolve_QTE(BattleQTEResult result)
{
    CHECK_NULL(_qteController);

    if (!_qteController->Resolve(result))
        return;

    OnQTEResolved.Broadcast(result);
}

void BattleController::Fail_QTEIfActive()
{
    CHECK_NULL(_qteController);

    if (!_qteController->FailIfActive())
        return;

    OnQTEResolved.Broadcast(BattleQTEResult::Fail);
}

void BattleController::Clear_QTEState()
{
    if (_qteController)
        _qteController->Clear();
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

    _buffSystem->Apply_Buff(target, buffId, stack, durationTurn);
}

bool BattleController::Has_Buff(const BattleChar& target, Buffs buffId) const
{
    return _buffSystem != nullptr && _buffSystem->Has_Buff(target, buffId);
}

bool BattleController::Is_GroupParryActive() const
{
    return _reactionController.Is_GroupParryActive();
}

void BattleController::Notify_CounterAnnounceRequested(const vector<BattleChar>& counterActors)
{
    if (counterActors.empty())
        return;

    OnCounterAnnounceRequested.Broadcast(counterActors);
}

void BattleController::StartCounterReturnAll(ModelCom* sourceModel)
{
    _reactionController.StartCounterReturnAll(sourceModel);
}

void BattleController::Play_BattleHitStop(float worldTimeScale, float duration)
{
    if (duration <= 0.f)
        return;

    GAME->Slow_WorldTime(worldTimeScale, duration);
}

HRESULT BattleController::Ready_BattleTurnCamera()
{
    if (_battleTurnCamera == nullptr)
    {
        auto camera = GAME->Get_CameraByType(ObjectType::BattleTurnCamera);

        if(camera)
            _battleTurnCamera = dynamic_pointer_cast<BattleTurnCamera>(camera);
    }

    if(_battleTurnCamera == nullptr)
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
    _battleTurnCamera->Set_Participants(_heroes, _enemies, _pendingBattleMonsterSpawnDescs);

    return S_OK;
}

void BattleController::Activate_BattleTurnCamera()
{
    auto camera = GAME->Get_CameraByType(ObjectType::BattleTurnCamera);

    if(camera)
        _battleTurnCamera = dynamic_pointer_cast<BattleTurnCamera>(camera);

    if (_battleTurnCamera == nullptr)
        return;

    BattleTurnCamera::BattleShotRequest request;
    request.mode = BattleCameraMode::BATTLEINTRO;
    request.subject = _enemies[_enemies.size() /2].actor;
    request.useClip = true;
    request.presetId = _enemies[_enemies.size()/2].actor->Get_SourcePrefabName();
    request.clipId = _enemies[_enemies.size()/2].actor->Get_SourcePrefabName();
    _battleTurnCamera->Apply_ShotRequest(request);

    const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    LOG_INFO(
        "[BattleCamTrace] BattleController::Activate_BattleTurnCamera activeBefore={} enemyCount={} subjectPrefab={} worldScale={:.3f}",
        static_cast<int>(activeCamera ? activeCamera->Get_ObjectType() : ObjectType::None),
        _enemies.size(),
        request.presetId.c_str(),
        GAME->Get_WorldTimeScale());

    CLIENT->Request_BattleTurn();
}

void BattleController::Restore_WorldCamera()
{
    CLIENT->Restore_PreviousCamera();
    GAME->Set_PlayerInputBlocked(false);
}

void BattleController::Clear_BattleRuntimeState()
{
    _turnAction = {};
    _turnActionRequest = {};
    _currentTurnActor = {};
    _pendingNextTurn = false;
    _requestEndTurnOnCurrentActorDeath = false;
    _reactionController.Clear_RuntimeState();
    Clear_QTEState();
    _isFreeAimShotPlaying = false;
    _freeAimShotAction = {};
    Clear_FreeAimHitTarget();
    currentTurnIndex = 0;
    _battleTime = 0.f;
    _flag = false;
    _isActionExecuting = false;
    _hitSequenceRunner.Clear();

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
    const bool isPlayerAdvantage = _pendingBattleIsPlayerAdvantage;

    // 현제 케릭 아이디 , 적 아이디로 전투 용 객체 생성
    Create_BattleChar(_pendingBattleHeroIds, _pendingBattleMonsterSpawnDescs);

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
        return;
    }

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
    constexpr float freeAimWeakPointBonusDamage = 11.f;

    DamageSystem::ApplyDamage(
        freeAimWeakPointBonusDamage,
        attacker,
        hitTarget.target,
        AttackType::FreeAimWeak);

    return true;
}

void BattleController::Configure_BattlePlayerSkills(HeroId heroId, const Shared<CombatStatCom>& battleStat,
    const Shared<SkillTreeCom>& worldSkillTreeCom)
{
    if (battleStat == nullptr)
        return;

    battleStat->Clear_Skills();

    const vector<string> defaultSkillKeys = Get_DefaultBattleSkillKeys(heroId);
    for (const string& skillKey : defaultSkillKeys)
    {
        if (skillKey.empty())
            continue;

        const Skill_Data* skillData = CLIENT->Get_Skill(skillKey);
        if (skillData == nullptr)
        {
            LOG_WARN("스킬 데이터 없음. heroId={}, skillKey={}",
                magic_enum::enum_name(heroId), skillKey.c_str());
            continue; // data가 없으면 battle slot에 넣지 않는다.
        }

        if (worldSkillTreeCom && !worldSkillTreeCom->IsUnlocked(skillKey))
            continue;

        battleStat->Set_Skill(skillKey);
    }

}

vector<string> BattleController::Get_DefaultBattleSkillKeys(HeroId heroId)
{
    // TODO : TEMP TEMP TEMP TEMP TEMP
    // 나중에, 해금된 스킬에 EquipSkillCom? 뭐 이런거 만들어서 장착된 스킬 기준으로 가져오기
    switch (heroId)
    {
    case HeroId::Gustave:
        return { "LumiereAssault", "UnleashedCharge", "MarkingShot" };
    case HeroId::Lune:
        return { "Immolation", "IceGust", "Thunderfall" };
    case HeroId::Maelle:
        return { "OffensiveSwitch", "Percee", "FleureFury" };
    default:
        return {};
    }
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

void BattleController::Set_BattleFSM()
{
    _battleFSM = BattleStateMachine::Create();

    _battleFSM->Add_State(BattleTurnState::ChooseSkill, ChooseSkill::Create(GetSharedPtr<BattleController>()));
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

void BattleController::End_FirstAnim()
{
    _firstAnimCount++;
    if (_firstAnimCount == 3)
    {
        for (const auto it : _heroes)
            it.ModelCom->Set_RootMotionEnabled(true);
        _firstAnimflag = true;
    }
}

void BattleController::PlayerStartAnim()
{
    _firstAnimflag = false;
    _firstAnimCount = 0;

    for (const auto it : _heroes)
    {
        it.ModelCom->Set_RootMotionEnabled(false);
        it.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Battle_Start), 0.3f, false);
    }

    for (const auto it : _enemies)
    {
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

    // 크리티컬이면 Heavy
    if (isCritical)
    {
        target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Hit_Heavy), 0.3f, false);
        return;
    }

    switch (attackType)
    {
    case AttackType::FreeAimLight:
    case AttackType::Combo:
        target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Hit_Light), 0.3f, false);
        break;

    case AttackType::BaseAttack:
    case AttackType::SingleSkill:
        target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Hit_Medium), 0.3f, false);
        break;

    case AttackType::SingleAOE:
    case AttackType::FreeAimWeak:
    case AttackType::ComboLast:
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

    Base::Free();
}
