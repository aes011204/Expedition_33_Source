#include "BattleController.h"

#include "BuffSystem.h"
#include "Character.h"

#include "AttackTargeting.h"
#include "BattleStateMachine.h"
#include "ChooseAction.h"
#include "ChooseItem.h"
#include "ChooseSkill.h"
#include "ChooseUltimate.h"
#include "CombatStatCom.h"
#include "Create_Class.h"
#include "ItemTargeting.h"
#include "SkillTargeting.h"
#include "Spawn_Prefab.h"
#include "UltimateTargeting.h"
#include "SkillScript.h"

HRESULT BattleController::Inintialize()
{
    Set_BattleFSM();
    return S_OK;

}

void BattleController::Update(float timeDelta)
{

    
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


}

void BattleController::OnEnterBattleField(uint32 FieldID, bool IsFirstHero, vector<string>& PlayerProtoTag,vector<string>& MonsterProtoTag)
{
    // 현제 케릭 아이디 , 적 아이디로 전투 용 객체 생성
    Create_BattleChar(PlayerProtoTag, MonsterProtoTag);

    // 초기 플레이 순서 지정
    Build_InitialTurnQueue(IsFirstHero);

    // 들어온 캐릭터들의 위치 셋팅
    // FieldID로 
    SetCharInitPos();

    // 턴 시작
    StartTurn();
}

EBattleEndResult BattleController::OnExitBattleField(EBattleEndResult result)
{
    // 도망치기
    // 실패
    // 성공 
    return result;
}

void BattleController::ExecuteAction(Action turnAction)
{
    auto BattleChar = turnAction._curChar;
    if (!BattleChar.actor)
        return;
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
    for(int i = 0 ; i < _heroes.size(); i++)
    {
        if (_heroes[i].statCom->Get_AliveStatus() == EAliveStatus::Dead)
            DeadPlayer++;
    }
    if (DeadPlayer == _heroes.size())
        OnExitBattleField(EBattleEndResult::Defeat);

    for (int i = 0; i < _enemies.size(); i++)
    {
        if (_enemies[i].statCom->Get_AliveStatus() == EAliveStatus::Dead)
            DeadEnemy++;
    }
    if (DeadEnemy == _enemies.size())
        OnExitBattleField(EBattleEndResult::Victory);


    // 상태 이상 엔드
    _buffSystem->ProcessTurnEnd(_currentTurnActor.statCom);

    // 다음 턴 시작

    auto actor = _currentTurnActor;
    // 죽은애들은 큐에 다시 안들어가게
    if(actor.statCom->Get_AliveStatus() != EAliveStatus::Dead)
    {
        
    TurnEntry entry{};
    entry.hero = actor;

    entry.nextTime =
        _battleTime + (100.f / max(actor.statCom->Get_Speed(), 1.f));

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
    auto actor = _currentTurnActor;

    // 상태 이상 엔드
    _buffSystem->ProcessTurnStart(_currentTurnActor.statCom);


    // fsm
    if (actor.statCom->Is_Player())
    {
        _battleFSM->Change_State(BattleTurnState::ChooseAction);
    }
    else
    {
        Run_EnemyTurn();
    }


}

void BattleController::Create_BattleChar(vector<string>& PlayerprotoTag, vector<string>& MonsterprotoTag)
{


    for (int i = 0; i < PlayerprotoTag.size(); i++)
    {

        Shared<GameObject> charater = Client::Spawn_Prefab::Spawn(
            PlayerprotoTag[i],
            GAME->Current_LevelIndex(),
            L"BattleLayer");

        if (charater == nullptr)
            LOG_WARN({ "Failed to Create BattleChar" });

        BattleChar battle_char = {};
        battle_char.statCom = charater->Get_Component<CombatStatCom>();
        battle_char.actor = dynamic_pointer_cast<Character>(charater);
    
            _heroes.push_back(battle_char);
    };

    for (int i = 0; i < MonsterprotoTag.size(); i++)
    {

        Shared<GameObject> charater = Client::Spawn_Prefab::Spawn(
            MonsterprotoTag[i],
            GAME->Current_LevelIndex(),
            L"BattleLayer");

        if (charater == nullptr)
            LOG_WARN({ "Failed to Create BattleChar" });

        BattleChar battle_char = {};
        battle_char.statCom = charater->Get_Component<CombatStatCom>();
        battle_char.actor = dynamic_pointer_cast<Character>(charater);

            _enemies.push_back(battle_char);
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
           if(!hero.actor)
               continue;

           TurnEntry entry = {};
           entry.nextTime = (100.f / max(hero.statCom->Get_Speed(), 1.f))+heroOffset;
           entry.hero = hero;
           _turnQueue.push(entry);
       }
       for (auto& enemy : _enemies)
       {
           if (!enemy.actor)
               continue;

           TurnEntry entry = {};
           entry.nextTime = (100.f / max(enemy.statCom->Get_Speed(), 1.f))+monsterOffset;
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
        if (!hero.actor)
            continue;

        hero.actor->Get_Transform()->Set_Position(
            PPos[i]);
    }

    for (size_t i = 0; i < _enemies.size(); ++i)
    {
        auto& enemy = _enemies[i];
        if (!enemy.actor)
            continue;

        enemy.actor->Get_Transform()->Set_Position(
            EPos[i]);
    }

}

void BattleController::Execute_BaseAttack(const BattleChar actor, const Action& turnAction)
{
    //기본스킬도 특이하기때문에 스킬로


   // auto script = Create_Class::SkillScript<Base_Gustave_Attack>().Create();
    auto script = Create_Class::SkillScript(turnAction._selected_SkillKey.scriptClassName).Create();

    if (script == nullptr)
    {
        LOG_ERROR("Base_Gustave_Attack Create Failed");
    }
    else
    {
        LOG_ERROR("Base_Gustave_Attack Create Success");
    }
\

    auto target = turnAction.target;

    script->Execute(turnAction._selected_SkillKey, actor, target);



}

void BattleController::Execute_Skill(const BattleChar actor, const Action& turnAction)
{
    const Skill_Data& skill = turnAction._selected_SkillKey;
 /*   auto script = Create_Class::
        skill.scriptClassName);*/

    auto target = turnAction.target;

    //script->Execute(skill, actor, target);
}

void BattleController::Execute_Item(const BattleChar actor, const Action& turnAction)
{
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
