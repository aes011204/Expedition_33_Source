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


    return S_OK;

}

void BattleController::Update(float timeDelta)
{
    if (_battleEnded == true)
    {
        return;
    }

    if (_pendingNextTurn)
    {
        _pendingNextTurn = false;
        StartTurn();
        return;
    }

    // 일단 지금은 배틀이랑 구분 안되어 있음 애니메이션 일단 돌리고 있음

    if (_heroes.empty() == true || _enemies.empty() == true)
        return;
    // 적턴 인지 플레이어 턴인지에 따라
    if (_currentTurnActor.statCom->Is_Player())
    {
        _battleFSM->Update_StateMachine(timeDelta);
    }



}
void BattleController::Run_EnemyTurn()
{
    LOG_DEBUG("Run_EnemyTurn: actor={}",
        Get_TurnAction()._curChar.actor ? String::ToString(Get_TurnAction()._curChar.actor->Get_Name()).c_str() : "null");

    auto monster
        = dynamic_pointer_cast<Monster_BattleBase>(Get_TurnAction()._curChar.actor);

    CHECK_NULL(monster);

    monster->Decide_Action(Get_TurnAction(), Get_AliveHeroes());
    ExecuteAction( Get_TurnAction());
}

void BattleController::OnEnterBattleField(uint32 FieldID, bool IsFirstHero, const vector<HeroId>& heroIds,
    vector<string>& battleMonsterPrefabNames)
{
	if (_isBattleActive)
        return;

    _isBattleActive = true;
    _battleEnded = false;
    _pendingNextTurn = false;

	// 월드 캐릭터 입력 막기
	GAME->Set_PlayerInputBlocked(true);
	_worldCameraBeforeBattle = GAME->Get_ActiveCamera();

    // 현제 케릭 아이디 , 적 아이디로 전투 용 객체 생성
    Create_BattleChar(heroIds, battleMonsterPrefabNames);

    // 초기 플레이 순서 지정
    Build_InitialTurnQueue(IsFirstHero);

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
		return;
	}

    Activate_BattleTurnCamera();

    // 턴 시작
    StartTurn();
}

EBattleEndResult BattleController::OnExitBattleField(EBattleEndResult result)
{
    _isBattleActive = false;

	Restore_WorldCamera(); // 카메라 복구

    // 도망치기
    // 실패
    // 성공

    _heroes.clear();
    _enemies.clear();
    return result;
}

void BattleController::ExecuteAction(Action turnAction)
{
    auto BattleChar = turnAction._curChar;
    if (!BattleChar.actor)
        return;

    LOG_DEBUG("ExecuteAction: actor={}, type={}",
        turnAction._curChar.actor ? String::ToString(turnAction._curChar.actor->Get_Name()).c_str() : "null",
        static_cast<int>(turnAction.type));

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

    LOG_DEBUG("EndTurn: actor={}",
        _currentTurnActor.actor ? String::ToString(_currentTurnActor.actor->Get_Name()).c_str() : "null");
    LOG_DEBUG("EndTurn: deadPlayer={}, deadEnemy={}", DeadPlayer, DeadEnemy);

    if (DeadEnemy == _enemies.size())
    {
        OnExitBattleField(EBattleEndResult::Victory);
        _battleEnded = true;

		return;
    }


    // 상태 이상 엔드
    _buffSystem->ProcessTurnEnd(_currentTurnActor.statCom);

    // 다음 턴 시작

    auto actor = _currentTurnActor;
    // 죽은애들은 큐에 다시 안들어가게
    if (actor.statCom->Get_AliveStatus() != EAliveStatus::Dead)
    {

        TurnEntry entry{};
        entry.hero = actor;

        entry.nextTime =
            _battleTime + (100.f / max(actor.statCom->Get_Speed(), 1.f));

        _turnQueue.push(entry);
    }



    _pendingNextTurn = true;



}

void BattleController::StartTurn()
{
    _currentTurnActor = _turnQueue.top().hero;
    _battleTime = _turnQueue.top().nextTime;
    _turnQueue.pop();

    LOG_DEBUG("StartTurn: next actor={}, isPlayer={}",
        _currentTurnActor.actor ? String::ToString(_currentTurnActor.actor->Get_Name()).c_str() : "null",
        _currentTurnActor.statCom ? _currentTurnActor.statCom->Is_Player() : 0);


    _turnAction = {};
    _turnAction._curChar = _currentTurnActor;
    auto actor = _currentTurnActor;

    // 상태 이상 엔드
    _buffSystem->ProcessTurnStart(_currentTurnActor.statCom);


    // fsm
    if (actor.statCom->Is_Player())
    {
        LOG_DEBUG("StartTurn: player turn -> FSM ChooseAction");
        _battleFSM->Change_State(BattleTurnState::ChooseAction);
    }
    else
    {
        LOG_DEBUG("StartTurn: enemy turn -> Run_EnemyTurn");
        Run_EnemyTurn();
    }


}

void BattleController::Create_BattleChar(const vector<HeroId>& heroIds, vector<string>& battleMonsterPrefabNames)
{
    for (HeroId heroId : heroIds)
    {
        const HeroSpawnDesc* spawnDesc = CLIENT->Find_HeroSpawnDesc(heroId);
        if (spawnDesc == nullptr)
            continue;

        Shared<GameObject> character = Spawn_Prefab::Spawn(
            spawnDesc->battlePrefabName,
            GAME->Current_LevelIndex(),
            L"BattleLayer");

        CHECK_NULL(character);

        auto ply = dynamic_pointer_cast<Character>(character);

        BattleChar battleChar{};
        battleChar.actor = ply;
        battleChar.statCom = character->Get_Component<CombatStatCom>();

        PartObject* body = ply ? ply->Get_PartObject(L"Part_Body") : nullptr;
        battleChar.ModelCom = body ? body->Get_Component<ModelCom>() : nullptr; // body part에 붙은 실제 ModelCom를 사용한다.

        _heroes.push_back(battleChar);
    }

    for (int i = 0; i < battleMonsterPrefabNames.size(); ++i)
    {
        Shared<GameObject> enemy = Spawn_Prefab::Spawn(
            battleMonsterPrefabNames[i],
            GAME->Current_LevelIndex(),
            L"BattleLayer");

        CHECK_NULL(enemy);

        auto ply = dynamic_pointer_cast<Character>(enemy);
        BattleChar battleChar{};
        battleChar.actor = ply;
        battleChar.statCom = enemy->Get_Component<CombatStatCom>();

        PartObject* body = ply ? ply->Get_PartObject(L"Part_Body") : nullptr;
        battleChar.ModelCom = body ? body->Get_Component<ModelCom>() : nullptr; // 몬스터 종류와 무관하게 Part_Body의 ModelCom를 사용한다.

        _enemies.push_back(battleChar);
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
    {
        heroOffset = 5;
    }
    else
    {
        monsterOffset = 5.f;
    }


    for (auto& hero : _heroes)
    {
        if (!hero.actor)
            continue;

        TurnEntry entry = {};
        entry.nextTime = (100.f / max(hero.statCom->Get_Speed(), 1.f)) + heroOffset;
        entry.hero = hero;
        _turnQueue.push(entry);
    }
    for (auto& enemy : _enemies)
    {
        if (!enemy.actor)
            continue;

        TurnEntry entry = {};
        entry.nextTime = (100.f / max(enemy.statCom->Get_Speed(), 1.f)) + monsterOffset;
        entry.hero = enemy;
        _turnQueue.push(entry);
    }


}


void BattleController::SetCharInitPos()
{
    /*INFO = FiledMgr->GetFiledInfo(FieldID)
    INFO.
    */
    Vec3 startPos = Vec3(225.f, -4020.5f, -1.24f);

    Vec3 EPos[3] = { {0.f,0.f,0.f},{10.f,0.f,0.f},{20.f,0.f,0.f} };
    Vec3 PPos[3] = { {0.f,0.f,10.f},{10.f,0.f,10.f},{20.f,0.f,10.f} };

    // 임시
    for (size_t i = 0; i < _heroes.size(); ++i)
    {
        auto& hero = _heroes[i];
        if (!hero.actor)
            continue;

        hero.actor->Get_Transform()->Set_Position(startPos +
            PPos[i]);
    }

    for (size_t i = 0; i < _enemies.size(); ++i)
    {
        auto& enemy = _enemies[i];
        if (!enemy.actor)
            continue;

        enemy.actor->Get_Transform()->Set_Position(startPos +
            EPos[i]);
    }

}

vector<BattleChar> BattleController::Get_AliveEnemies()
{

    vector<BattleChar> alive;
    for (int i = 0; i < _enemies.size(); i++)
    {
        if (_enemies[i].statCom->Get_CurAliveState() == EAliveStatus::Alive)
        {
            alive.push_back(_enemies[i]);
        }
    }
    return  alive;
}

vector<BattleChar> BattleController::Get_AliveHeroes()
{
    vector<BattleChar> alive;
    for (int i = 0; i < _heroes.size(); i++)
    {
        if (_heroes[i].statCom->Get_CurAliveState() == EAliveStatus::Alive)
        {
            alive.push_back(_heroes[i]);
        }
    }
    return  alive;
}

void BattleController::Handle_EditorStopRuntime()
{
    _turnAction = {};
    _currentTurnActor = {};
    _pendingNextTurn = false;
    _battleEnded = false;
    _isBattleActive = false;
    currentTurnIndex = 0;
    _battleTime = 0.f;
    _flag = false;

    while (!_turnQueue.empty())
        _turnQueue.pop();

    _heroes.clear();
    _enemies.clear();

    if (_battleFSM)
        _battleFSM->Change_State(BattleTurnState::ChooseAction);
}

HRESULT BattleController::Ready_Delegates()
{
    _editorStopHandle = GAME->Get_DelegateHub().OnEditorStopRuntime.Add(
        [this]()
        {
            Handle_EditorStopRuntime();
        });

    return S_OK;
}

void BattleController::Execute_BaseAttack(const BattleChar actor, const Action& turnAction)
{
    //기본스킬도 특이하기때문에 스킬로
    LOG_DEBUG("Execute_BaseAttack: actor={}, target={}, script={}",
        actor.actor ? String::ToString(actor.actor->Get_Name()).c_str() : "null",
        turnAction.target.actor ? String::ToString(turnAction.target.actor->Get_Name()).c_str() : "null",
        turnAction._selected_SkillKey.scriptClassName.c_str());

    auto vec_anim = turnAction._selected_SkillKey.animationKey;
    if (vec_anim.size() == 0)
        return;

    if (actor.ModelCom == nullptr)
    {
        LOG_ERROR("Execute_BaseAttack: actor.ModelCom is null");
        EndTurn();
        return;
    }

    if (vec_anim.empty())
    {
        LOG_ERROR("Execute_BaseAttack: no animationKey");
        EndTurn();
        return;
    }

    //turnAction._curChar.ModelCom->Set_Animation(vec_anim[0], false, 1.5f);
    //vec_anim


    //auto script = Create_Class::SkillScript(turnAction._selected_SkillKey.scriptClassName).Create();
    //if (script == nullptr)
    //{
    //    LOG_ERROR("skill Create Failed");
    //}
    //else
    //{
    //    LOG_ERROR("skill Create Success");
    //}
    //
    //auto target = turnAction.target;
    //
    //script->Execute(turnAction._selected_SkillKey, actor, target);
    //LOG_DEBUG("Execute_BaseAttack: script finished");
    //
    //EndTurn();// 일단 애니메이션등이 다 끝나고 끝내야 하는데 일단 테스트

}

void BattleController::Execute_Skill(const BattleChar actor, const Action& turnAction)
{
    LOG_DEBUG("Execute_Skill: actor={}, target={}, script={}",
        actor.actor ? String::ToString(actor.actor->Get_Name()).c_str() : "null",
        turnAction.target.actor ? String::ToString(turnAction.target.actor->Get_Name()).c_str() : "null",
        turnAction._selected_SkillKey.scriptClassName.c_str());

    auto script = Create_Class::SkillScript(turnAction._selected_SkillKey.scriptClassName).Create();

    if (script == nullptr)
    {
        LOG_ERROR("Base_Gustave_Attack Create Failed");
    }
    else
    {
        LOG_ERROR("Base_Gustave_Attack Create Success");
    }
 

        auto target = turnAction.target;

    script->Execute(turnAction._selected_SkillKey, actor, target);

    EndTurn();// 일단 애니메이션등이 다 끝나고 끝내야 하는데 일단 테스트

}

void BattleController::Execute_Item(const BattleChar actor, const Action& turnAction)
{
}

void BattleController::OnActionEndNotify()
{
    auto& action = Get_TurnAction();
    
    // 다음 애니로 이동
    action._selected_SkillKey._curAnimIndex++;
    
    if (action._selected_SkillKey._curAnimIndex >= action._selected_SkillKey.animationKey.size())
    {
        // 끝났으면 Idle
        string key = action._selected_SkillKey.IdleAnimationKey;
        action._curChar.ModelCom->Set_Animation(key, false, 1.5f);
    
        EndTurn();
        return;
    }
    
    // 다음 애니 실행
    string key = action._selected_SkillKey.animationKey[action._selected_SkillKey._curAnimIndex];
    action._curChar.ModelCom->Set_Animation(key, false, 1.5f);

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
	if (_worldCameraBeforeBattle)
        GAME->Change_ActiveCamera(_worldCameraBeforeBattle);

    GAME->Set_PlayerInputBlocked(false);
    _worldCameraBeforeBattle.reset();
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
