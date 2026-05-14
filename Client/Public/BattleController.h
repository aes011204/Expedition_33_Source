#pragma once
#include "Base.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)

    class Character;

NS_END


NS_BEGIN(Client)

class CombatStatCom;
class BuffSystem;
class BattleEffectResolver;

    class BattleStateMachine;
class BattleController :
    public Base
{

public:
 
public:
    HRESULT Inintialize();
    void Update(float timeDelta);



    void Run_EnemyTurn();


    void  OnEnterBattleField(uint32 FieldID, bool IsFirstHero, vector<string>& PlayerProtoTag, vector<string>& MonsterProtoTag);
    EBattleEndResult OnExitBattleField(EBattleEndResult result);

    void Set_BattleFSM();
    void  ExecuteAction(Action turnAction); //실제로 선택한 행동을 실행하는 함수

    void EndTurn();
    void StartTurn();

    EAPChangeReason FinishBattle();



    void Create_BattleChar(vector<string>& PlayerprotoTag, vector<string>& MonsterprotoTag);
    void Build_InitialTurnQueue(bool IsFirstHero);
    void SetCharInitPos();

public:
    Shared< BattleStateMachine>  Get_BattleFSM() { return _battleFSM; };
    Action& Get_TurnAction() { return _turnAction; }
    uint32 Get_EnemiesNum() { return _enemies.size(); }
    uint32 Get_HeroesNum() { return _heroes.size(); }
    vector<BattleChar>& Get_Enemies() { return _enemies; }
private:
    BattleChar _currentTurnActor = {};
    Action _turnAction={};
    bool isBattleActive = { false };
    int currentTurnIndex = {};
     


private:
    void Execute_BaseAttack(const BattleChar actor, const Action& turnAction);
    void Execute_Skill(const BattleChar actor, const Action& turnAction);
    void Execute_Item(const BattleChar actor, const Action& turnAction);

public:
    struct Compare
    {
        bool operator()(TurnEntry& a, TurnEntry& b)
        {
            if (a.nextTime > b.nextTime)
                  // Ultimate_Data    _selected_ultimate
                return true;
             else
                return false;
        }
    };
private:
    priority_queue<TurnEntry, vector<TurnEntry>, Compare> _turnQueue;
    float _battleTime = {};
    vector<BattleChar> _heroes;
    vector<BattleChar> _enemies;


    Shared< BattleStateMachine> _battleFSM;
    Shared< BuffSystem> _buffSystem;
    Shared< BattleEffectResolver> _battleEffectResolver;
};


NS_END
