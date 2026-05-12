#include "BattleController.h"
#include "Character.h"

#include "AttackTargeting.h"
#include "BattleStateMachine.h"
#include "ChooseAction.h"
#include "ChooseItem.h"
#include "ChooseSkill.h"
#include "ChooseUltimate.h"
#include "ItemTargeting.h"
#include "SkillTargeting.h"
#include "Spawn_Prefab.h"
#include "UltimateTargeting.h"

HRESULT BattleController::Inintialize()
{
    Set_BattleFSM();
    return S_OK;

}

void BattleController::Update(float timeDelta)
{

    
       if (_heros.empty() == true|| _enemies.empty() == true)
            return;
       // 적턴 인지 플레이어 턴인지에 따라
        if (_currentTurnActor.lock()->IsPlayer())
        {
            _battleFSM->Update_StateMachine(timeDelta);
        }



}
void BattleController::Run_EnemyTurn()
{
}

void BattleController::OnEnterBattleField(uint32 FieldID, bool IsFirstHero, vector<string>& protoTag)
{
    // 현제 케릭 아이디 , 적 아이디로 전투 용 객체 생성
    Create_BattleChar(protoTag);

    // 초기 플레이 순서 지정
    Build_InitialTurnQueue(IsFirstHero);

    // 들어온 캐릭터들의 위치 셋팅
    SetCharInitPos();

    // 턴 시작
    StartTurn();
}

EBattleEndResult BattleController::OnExitBattleField(EBattleEndResult result)
{
    // 도망치기
    // 실패
    // 성공 

}

void BattleController::ExecuteAction(Action turnAction)
{
    auto actor = turnAction._curChar.lock();
    if (!actor)
        return;

    switch (turnAction.type)
    {
    case EBattleContext::BaseAttack:
        Execute_BaseAttack(actor, turnAction);
        break;

    case EBattleContext::Skill:
        Execute_Skill(actor, turnAction);
        break;

    case EBattleContext::ConsumableItem:
        Execute_Item(actor, turnAction);
        break;
    }
}

void BattleController::EndTurn()
{
    // 끝났다면 끝
    uint32 DeadPlayer = {};
    uint32 DeadEnemy = {};
    for(int i = 0 ; i < _heroes.size(); i++)
    {
        if (_heroes[i]->Get_AliveStatus() == EAliveStatus::Dead)
            DeadPlayer++;
    }
    if (DeadPlayer == _heroes.size())
        OnExitBattleField(EBattleEndResult::Defeat);

    for (int i = 0; i < _enemies.size(); i++)
    {
        if (_enemies[i]->Get_AliveStatus() == EAliveStatus::Dead)
            DeadEnemy++;
    }
    if (DeadEnemy == _enemies.size())
        OnExitBattleField(EBattleEndResult::Victory);

    // 다음 턴 시작

    auto actor = _currentTurnActor.lock();
    // 죽은애들은 큐에 다시 안들어가게
    if(actor->Get_AliveStatus() != EAliveStatus::Dead)
    {
        
    TurnEntry entry{};
    entry.hero = actor;

    entry.nextTime =
        _battleTime + (100.f / max(actor->Get_Speed(), 1.f));

    _turnQueue.push(entry);
    }




    StartTurn();

}

void BattleController::StartTurn()
{

   
    _currentTurnActor = _turnQueue.top().hero;
    _battleTime = _turnQueue.top().nextTime;
    _turnQueue.pop();


    _turnAction = {};
    _turnAction._curChar = _currentTurnActor;
    auto actor = _currentTurnActor.lock();

    if (actor->IsPlayer())
    {
        _battleFSM->Change_State(BattleTurnState::ChooseAction);
    }
    else
    {
        Run_EnemyTurn();
    }
}

void BattleController::Create_BattleChar(vector<string>& protoTag)
{


    for (int i = 0; i < protoTag.size(); i++)
    {

        Shared<GameObject> Charter = Client::Spawn_Prefab::Spawn(
            protoTag[i],
            GAME->Current_LevelIndex(),
            L"BattleLayer");

        if (Charter == nullptr)
            LOG_WARN({ "Failed to Create BattleChar" });

        if (dynamic_point_case<Pl>(Charter)->IsPlayer())
        {
            _heroes.push_back(Charter);
        }
        else
        {
            
        }
    };


}

void BattleController::Build_InitialTurnQueue(bool IsFirstHero)
{

    float heroOffset ={};
    float monsterOffset = {};
    // 첨 초기화
    _battleTime = 0;

    while(!_turnQueue.empty())
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


       for(auto& hero :_heroes)
       {
           if(!hero)
               continue;

           TurnEntry entry = {};
           entry.nextTime = (100.f / max(hero->Get_Speed(), 1.f))+heroOffset;
           entry.hero = hero;
           _turnQueue.push(entry);
       }
       for (auto& enemy : _enemies)
       {
           if (!enemy)
               continue;

           TurnEntry entry = {};
           entry.nextTime = (100.f / max(enemy->Get_Speed(), 1.f))+monsterOffset;
           entry.hero = enemy;
           _turnQueue.push(entry);
       }


}


void BattleController::SetCharInitPos()
{
    /*INFO = FiledMgr->GetFiledInfo(FieldID)
    INFO.
    */

    Vec3 EPos[3] = { {0.f,0.f,0.f},{10.f,0.f,0.f},{20.f,0.f,0.f} };
    Vec3 PPos[3] = { {0.f,20.f,0.f},{10.f,20.f,0.f},{20.f,20.f,0.f} };

    // 임시
    for (size_t i = 0; i < _heroes.size(); ++i)
    {
        auto& hero = _heroes[i];
        if (!hero)
            continue;

        hero->Get_Transform()->Set_Position(
            PPos[i]);
    }

    for (size_t i = 0; i < _enemies.size(); ++i)
    {
        auto& enemy = _enemies[i];
        if (!enemy)
            continue;

        enemy->Get_Transform()->Set_Position(
            EPos[i]);
    }

}

void BattleController::Execute_BaseAttack(const Shared<Character>& actor, const Action& turnAction)
{
    target->ApplyDamage(target, 10, EDamageReason::Attack);
    EndTurn();
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
