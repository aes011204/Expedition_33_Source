#include "BattleController.h"
#include "BuffSystem.h"
#include "Character.h"
#include "ClientInstance.h"
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
#include "Spawn_Prefab.h"
#include "UltimateTargeting.h"
#include "SkillScript.h"
#include "ModelCom.h"
#include "PartObject.h"
#include "BattleTurnCamera.h"
#include "Camera.h"
#include "TargetCamera.h"
#include "BattleActionExecutor.h"

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

    return S_OK;
}

void BattleController::Update(float timeDelta)
{
    if (_isBattleEntering)
    {
        // 슬로우 모션 지속시간은 월드 배속 안받은 Unscaled Time 가져오기
        _battleEnterElapsed += GAME->Get_UnscaledTimeDelta();

        if (_battleEnterElapsed >= _battleEnterDuration)
            Complete_BattleField();

        return;
    }

    if (_isWaitingBattleTransition)
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

    // 패링, 회피 키인풋
    UpdateReactionInput();

    // 한턴 끝나면 여기로 
    if (_pendingNextTurn)
    {
        _battleActionExe->Set_ReStart();
        StartTurn();
        _pendingNextTurn = false;
        return;
    }


    if (_heroes.empty() == true || _enemies.empty() == true)
        return;
    // 적턴 인지 플레이어 턴인지에 따라
    if (_currentTurnActor.statCom->Is_Player())
        _battleFSM->Update_StateMachine(timeDelta);


    if (_turnAction._curChar.actor != nullptr && !_battleActionExe->IsFinished())
    {
        _battleActionExe->Update(timeDelta, _turnAction);
    }

    if (_turnAction._curChar.actor != nullptr && _battleActionExe->IsFinished())
    {
      
        EndTurn();
        return;
    }
}

void BattleController::Run_EnemyTurn()
{
    LOG_DEBUG(
        "Run_EnemyTurn: actor={}",
        Get_TurnAction()._curChar.actor ? String::ToString(Get_TurnAction()._curChar.actor->Get_Name()).c_str() : "null"
    );

    auto monster
        = dynamic_pointer_cast<Monster_BattleBase>(Get_TurnAction()._curChar.actor);

    CHECK_NULL(monster);

    monster->Decide_Action_BTOnly(Get_TurnAction(), Get_AliveHeroes());
    ExecuteAction( Get_TurnAction());
}

void BattleController::OnEnterBattleField(uint32 FieldID, bool IsFirstHero, const vector<HeroId>& heroIds,
    vector<string>& battleMonsterPrefabNames)
{
    if (_isBattleActive || _isBattleEntering)
        return;

    _isBattleEntering = true;
    _battleEnterElapsed = 0.f;
    _battleEnded = false;
    _pendingNextTurn = false;

    // 2초 뒤 실제 배틀 생성 데이터 추가
    _pendingBattleFieldID = FieldID;
    _pendingBattleIsFirstHero = IsFirstHero;
    _pendingBattleHeroIds = heroIds;
    _pendingBattleMonsterPrefabNames = battleMonsterPrefabNames;

	// 월드 캐릭터 입력 막기
	GAME->Set_PlayerInputBlocked(true);
	_worldCamera = GAME->Get_ActiveCamera();
    GAME->Slow_WorldTime(_battleEnterWorldTimeScale, _battleEnterDuration);
}

EBattleEndResult BattleController::OnExitBattleField(EBattleEndResult result)
{
    _isBattleActive = false;
    _isBattleEntering = false;
    _isWaitingBattleTransition = false;

    Clear_TargetPreview();
    OnBattleEnded.Broadcast(result); // battle end UI 정리는 GameplayUI_Battle에서 처리한다.

    // 전투 종료 시 슬로우모션 값이 남지 않도록 복구
    GAME->Stop_WorldTimeSlow();

	Restore_WorldCamera(); // 카메라 복구

    // 도망치기
    // 실패
    // 성공

    _heroes.clear();
    _enemies.clear();

    Clear_BattleEnterPending();

    return result;
}

void BattleController::ExecuteAction(Action turnAction)
{
    const auto BattleChar = turnAction._curChar;
    if (!BattleChar.actor)
        return;

    LOG_DEBUG(
        "ExecuteAction: actor={}, type={}",
        turnAction._curChar.actor ? String::ToString(turnAction._curChar.actor->Get_Name()).c_str() : "null",
        static_cast<int>(turnAction.type)
    );

    // 일반 공격도 스킬로 들어감 구분만 BaseAttack
    switch (turnAction.type)
    {
    case EBattleContext::BaseAttack:
        Execute_BaseAttack(BattleChar, turnAction);
        break;

    case EBattleContext::Skill:
        Execute_Skill(BattleChar, turnAction);
        break;

    case EBattleContext::ConsumableItem:
        Execute_Item(BattleChar, turnAction);
        break;
    }
}

void BattleController::EndTurn()
{
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
    _currentTurnActor = _turnQueue.top().hero;
    _battleTime = _turnQueue.top().nextTime;
    _turnQueue.pop();

    LOG_DEBUG(
        "StartTurn: next actor={}, isPlayer={}",
        _currentTurnActor.actor ? String::ToString(_currentTurnActor.actor->Get_Name()).c_str() : "null",
        _currentTurnActor.statCom ? _currentTurnActor.statCom->Is_Player() : 0
    );

    _turnAction = {};
    _turnAction._curChar = _currentTurnActor;
    const auto actor = _currentTurnActor;

    // 상태 이상 엔드
    _buffSystem->ProcessTurnStart(_currentTurnActor.statCom);

    // fsm
    if (actor.statCom->Is_Player())
    {
        LOG_DEBUG("StartTurn: player turn -> FSM ChooseAction");
        _battleFSM->Change_State(BattleTurnState::ChooseAction);
        OnTurnStarted.Broadcast(_currentTurnActor, BattleTurnState::ChooseAction);
    }
    else
    {
        LOG_DEBUG("StartTurn: enemy turn -> Run_EnemyTurn");
        OnTurnStarted.Broadcast(_currentTurnActor, BattleTurnState::None);
        Run_EnemyTurn();
    }
}

void BattleController::Create_BattleChar(const vector<HeroId>& heroIds, vector<string>& battleMonsterPrefabNames)
{
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

        auto ply = dynamic_pointer_cast<Character>(character);

        BattleChar battleChar{};
        battleChar.actor = ply;
        battleChar.statCom = character->Get_Component<CombatStatCom>();

        PartObject* body = ply ? ply->Get_PartObject(L"Part_Body") : nullptr;
        battleChar.ModelCom = body ? body->Get_Component<ModelCom>() : nullptr; // body part에 붙은 실제 ModelCom를 사용한다.

        _heroes.push_back(battleChar);


       // battleChar.ModelCom->Set_RootMotionEnabled(false);
    }

    for (int i = 0; i < battleMonsterPrefabNames.size(); ++i)
    {
        Shared<GameObject> enemy = Spawn_Prefab::Spawn(
            battleMonsterPrefabNames[i],
            GAME->Current_LevelIndex(),
            L"BattleLayer"
        );

        CHECK_NULL(enemy);

        auto ply = dynamic_pointer_cast<Character>(enemy);
        BattleChar battleChar{};
        battleChar.actor = ply;
        battleChar.statCom = enemy->Get_Component<CombatStatCom>();

        PartObject* body = ply ? ply->Get_PartObject(L"Part_Body") : nullptr;
        battleChar.ModelCom = body ? body->Get_Component<ModelCom>() : nullptr; // 몬스터 종류와 무관하게 Part_Body의 ModelCom를 사용한다.

        _enemies.push_back(battleChar);

      // battleChar.ModelCom->Set_RootMotionEnabled(false);

    }
}

void BattleController::Build_InitialTurnQueue(bool IsFirstHero)
{
    float heroOffset = {};
    float monsterOffset = {};
    // 첨 초기화
    _battleTime = 0;

    while (!_turnQueue.empty())
    {
        _turnQueue.pop();
    }

    //초기 보정 값

    if (IsFirstHero == false)
        heroOffset = 5;
    else
        monsterOffset = 5.f;

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

void BattleController::SetCharInitPos()
{
    /*INFO = FiledMgr->GetFiledInfo(FieldID)
    INFO.
    */
    const Vec3 startPos = Vec3(225.f, -4020.5f, -1.24f);

    const Vec3 playerPos[3] =
    {
        { -3.f, 0.f, 0.f },
        { 0.f, 0.f, 0.f },
        { 3.f, 0.f, 0.f },
    };

    const Vec3 enemyPos[3] =
    {
        { -3.f, 0.f, 10.f },
        { 0.f, 0.f, 10.f },
        { 3.f, 0.f, 10.f },
    };

    // 임시
    for (size_t i = 0; i < _heroes.size(); ++i)
    {
        const auto& hero = _heroes[i];
        if (!hero.actor)
            continue;

        hero.actor->Get_Transform()->Set_Position(startPos + playerPos[i]);
    }

    for (size_t i = 0; i < _enemies.size(); ++i)
    {
        const auto& enemy = _enemies[i];
        if (!enemy.actor)
            continue;

        enemy.actor->Get_Transform()->Set_Position(startPos + enemyPos[i]);
    }
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
    CLIENT->Stop_BattleEnterTransition();
    GAME->Stop_WorldTimeSlow();
    GAME->Set_WorldTimeScale(1.f);
    GAME->Set_PlayerInputBlocked(false);

    _turnAction = {};
    _currentTurnActor = {};
    _pendingNextTurn = false;
    _battleEnded = false;
    _isBattleActive = false;
    _isBattleEntering = false;
    _isWaitingBattleTransition = false;
    currentTurnIndex = 0;
    _battleTime = 0.f;
    _flag = false;

    while (!_turnQueue.empty())
    {
        _turnQueue.pop();
    }

    _heroes.clear();
    _enemies.clear();
    _battleTurnCamera.reset();

    Clear_BattleEnterPending();
    Restore_WorldCamera();

    if (_battleFSM)
        _battleFSM->Change_State(BattleTurnState::ChooseAction);
}

HRESULT BattleController::Ready_Delegates()
{
    _editorStopHandle = GAME->Get_DelegateHub().OnEditorStopRuntime.Add(
        [this]
        {
            Handle_EditorStopRuntime();
        }
    );

    return S_OK;
}

void BattleController::OpenReactionWindow(ReactionType type)
{
    _reaction.type = type;
    _reaction.isConsumed = false;
    _reaction.isOpen = true;

}

void BattleController::CloseReactionWindow(ReactionType type)
{
    if (_reaction.type != type)
        return;

    _reaction.type = ReactionType::None;
    _reaction.isOpen = false;
    _reaction.isConsumed = false;
}

void BattleController::ClearReactionState()
{
    _reaction.type = ReactionType::None;
    _reaction.isOpen = false;
    _reaction.isConsumed = false;
}

void BattleController::UpdateReactionInput()
{
    if (_turnAction.target.actor == nullptr)
        return;

    // 애님 딜래이 +
    if (_turnAction.target.statCom->Is_Player() != true)
        return;


    switch (_reaction.type)
    {
    case ReactionType::Parry:
        if (GAME->KeyDown(KEY_TYPE::E))
        {
            // 패링은 여기서 애니XX

            if (!_reaction.isOpen || _reaction.isConsumed)
                return;

            // 성공 해야 애니 실행

            _reaction.isConsumed = true;
            _reaction.result = Parry;
            LOG_ERROR({ "SUCCESS PARRY" });
        }
        break;

    case ReactionType::Dodge:
        if (GAME->KeyDown(KEY_TYPE::Q))
        {
            // 회피 애님 실행

            if (!_reaction.isOpen || _reaction.isConsumed)
                return;


            _reaction.isConsumed = true;
            _reaction.result = Dodge;

            LOG_ERROR({ "SUCCESS DODGE" });
        }
        break;

    default:
        break;
    }

}

void BattleController::Set_TargetPreview(const BattleChar& target)
{
    _targetPreview = target;
    _hasTargetPreview = (target.actor != nullptr && target.statCom != nullptr);

    if (_hasTargetPreview)
        OnTargetPreviewChanged.Broadcast(&_targetPreview); // Hp Bar/타겟 UI 갱신 등
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

void BattleController::Execute_BaseAttack(const BattleChar actor, const Action& turnAction)
{
    //기본스킬도 특이하기때문에 스킬로
    LOG_DEBUG(
        "Execute_BaseAttack: actor={}, target={}, script={}",
        actor.actor ? String::ToString(actor.actor->Get_Name()).c_str() : "null",
        turnAction.target.actor ? String::ToString(turnAction.target.actor->Get_Name()).c_str() : "null",
        turnAction._selected_SkillKey.scriptClassName.c_str()
    );

   // auto script = Create_Class::SkillScript<Base_Gustave_Attack>().Create();
    _curScript = Create_Class::SkillScript(_turnAction._selected_SkillKey.scriptClassName).Create();

    auto& action = Get_TurnAction();

    if (actor.ModelCom == nullptr)
    {
        LOG_ERROR("Execute_BaseAttack: actor.ModelCom is null");
        EndTurn();
        return;
    }
    if (action._selected_SkillKey.AnimPhases.empty())
    {
        LOG_ERROR("Execute_BaseAttack: no animationKey");
        EndTurn();
        return;
    }

    action.originPos = action._curChar.actor->Get_Transform()->Get_Position();
    action._curAnimIndex = 0;

    _battleActionExe->BeginAction(action);
}

void BattleController::Execute_Skill(const BattleChar actor, const Action& turnAction)
{
    LOG_DEBUG(
        "Execute_Skill: actor={}, target={}, script={}",
        actor.actor ? String::ToString(actor.actor->Get_Name()).c_str() : "null",
        turnAction.target.actor ? String::ToString(turnAction.target.actor->Get_Name()).c_str() : "null",
        turnAction._selected_SkillKey.scriptClassName.c_str()
    );

    _curScript = Create_Class::SkillScript(_turnAction._selected_SkillKey.scriptClassName).Create();

    auto& action = Get_TurnAction();

    if (actor.ModelCom == nullptr)
    {
        LOG_ERROR("Execute_BaseAttack: actor.ModelCom is null");
        EndTurn();
        return;
    }
    if (action._selected_SkillKey.AnimPhases.empty())
    {
        LOG_ERROR("Execute_BaseAttack: no animationKey");
        EndTurn();
        return;
    }

   action.originPos = action._curChar.actor->Get_Transform()->Get_Position();
    action._curAnimIndex = 0;

    _battleActionExe->BeginAction(action);
}

void BattleController::Execute_Item(const BattleChar actor, const Action& turnAction)
{
}

void BattleController::OnActionEndNotify()
{
    auto& action = Get_TurnAction();
    _battleActionExe->OnActionEndNotify(action);


}

HRESULT BattleController::Ready_BattleTurnCamera()
{
	if (_battleTurnCamera)
        return S_OK;

    BattleTurnCamera::BATTLE_TURN_CAMERA_DESC desc{};
    desc.eye = XMFLOAT4(0.f, 8.f, -15.f, 1.f);
    desc.at = XMFLOAT4(0.f, 1.2f, 0.f, 1.f);
    desc.fovy = XMConvertToRadians(45.f);
    desc.nearPlane = 0.1f;
    desc.farPlane = 500.f;
    desc.mouseSensor = 0.f;
    desc.mouseSensitivity = 0.f;

    Shared<GameObject> cameraObject;
    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::Static), TEXT("GameObject_BattleTurnCamera"),
            GAME->Current_LevelIndex(), TEXT("Layer_Camera"), cameraObject, &desc), E_FAIL);

    _battleTurnCamera = dynamic_pointer_cast<BattleTurnCamera>(cameraObject);
    CHECK_NULL(_battleTurnCamera, E_FAIL);

    _battleTurnCamera->Try_BeginPlay();

    return S_OK;
}

void BattleController::Activate_BattleTurnCamera()
{
    if (_battleTurnCamera == nullptr)
        return;

	_battleTurnCamera->Focus_Battle(_heroes, _enemies);
	GAME->Change_ActiveCamera(_battleTurnCamera);
}

void BattleController::Restore_WorldCamera()
{
	if (_worldCamera)
        GAME->Change_ActiveCamera(_worldCamera);

    GAME->Set_PlayerInputBlocked(false);
    _worldCamera.reset();
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

    const uint32 battleFieldId = _pendingBattleFieldID;
    const bool isFirstHero = _pendingBattleIsFirstHero;

    // 카메라 저장
    _worldCamera = GAME->Get_ActiveCamera();

    // 현제 케릭 아이디 , 적 아이디로 전투 용 객체 생성
    Create_BattleChar(_pendingBattleHeroIds, _pendingBattleMonsterPrefabNames);

    // 초기 플레이 순서 지정
    Build_InitialTurnQueue(_pendingBattleIsFirstHero);

    // 들어온 캐릭터들의 위치 셋팅
    // FieldID로 
    SetCharInitPos();

	if (FAILED(Ready_BattleTurnCamera()))
	{
		LOG_ERROR("Create Failed : Battle Camera");

		Restore_WorldCamera();
		_isBattleActive = false;
		_battleEnded = true;

		_heroes.clear();
		_enemies.clear();
        Clear_BattleEnterPending();
		return;
	}

    Activate_BattleTurnCamera();
    Clear_BattleEnterPending();

    // 턴 시작
    OnBattleStarted.Broadcast(battleFieldId, isFirstHero); // battle start event만 발행하고, 전환 연출은 GameplayUI_Battle이 처리한다.
    _isWaitingBattleTransition = true;
}

void BattleController::Clear_BattleEnterPending()
{
    _isBattleEntering = false;
    _battleEnterElapsed = 0.f;

    _pendingBattleFieldID = 0;
    _pendingBattleIsFirstHero = false;
    _pendingBattleHeroIds.clear();
    _pendingBattleMonsterPrefabNames.clear();
}

void BattleController::Set_BattleFSM()
{
    _battleFSM = BattleStateMachine::Create();

    _battleFSM->Add_State(BattleTurnState::ChooseSkill, ChooseSkill::Create(dynamic_pointer_cast<BattleController>(shared_from_this())));
    _battleFSM->Add_State(BattleTurnState::AttackTargeting, AttackTargeting::Create(dynamic_pointer_cast<BattleController>(shared_from_this())));
    _battleFSM->Add_State(BattleTurnState::ChooseItem, ChooseItem::Create(dynamic_pointer_cast<BattleController>(shared_from_this())));
    _battleFSM->Add_State(BattleTurnState::ChooseAction, ChooseAction::Create(dynamic_pointer_cast<BattleController>(shared_from_this())));
    _battleFSM->Add_State(BattleTurnState::SkillTargeting, SkillTargeting::Create(dynamic_pointer_cast<BattleController>(shared_from_this())));
    _battleFSM->Add_State(BattleTurnState::ItemTargeting, ItemTargeting::Create(dynamic_pointer_cast<BattleController>(shared_from_this())));
    _battleFSM->Add_State(BattleTurnState::UltimateTargeting, UltimateTargeting::Create(dynamic_pointer_cast<BattleController>(shared_from_this())));
    _battleFSM->Add_State(BattleTurnState::ChooseUltimate, ChooseUltimate::Create(dynamic_pointer_cast<BattleController>(shared_from_this())));

    //_battleFSM->Change_State(BattleTurnState::ChooseAction);
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

    Base::Free();
}
