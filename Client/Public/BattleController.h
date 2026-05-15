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

class BattleController : public Base
{

public:
    BattleController();
    virtual ~BattleController();
public:
    HRESULT Initialize();
    void Update(float timeDelta);

    void Run_EnemyTurn();

    void  OnEnterBattleField(
        uint32 FieldID,
        bool IsFirstHero,
        const vector<HeroId>& heroIds,
        vector<string>& MonsterProtoTag);

    EBattleEndResult OnExitBattleField(EBattleEndResult result);

    void Set_BattleFSM();
    void  ExecuteAction(Action turnAction); //실제로 선택한 행동을 실행하는 함수

    void EndTurn();
    void StartTurn();

    EAPChangeReason FinishBattle();

    void Create_BattleChar(const vector<HeroId>& heroIds, vector<string>& MonsterprotoTag);
    void Build_InitialTurnQueue(bool IsFirstHero);
    void SetCharInitPos();

public:
    Shared< BattleStateMachine>  Get_BattleFSM() { return _battleFSM; };
    Action& Get_TurnAction() { return _turnAction; }
    //uint32 Get_EnemiesNum() { return _enemies.size(); }
    //uint32 Get_HeroesNum() { return _heroes.size(); }

    vector<BattleChar> Get_AliveEnemies();
    vector<BattleChar> Get_AliveHeroes();

private:
    BattleChar _currentTurnActor = {};
    Action _turnAction={};
    bool isBattleActive = { false };
    int currentTurnIndex = {};
     
    bool _flag = { false };

private:
    void Execute_BaseAttack(const BattleChar actor, const Action& turnAction);
    void Execute_Skill(const BattleChar actor, const Action& turnAction);
    void Execute_Item(const BattleChar actor, const Action& turnAction);


    bool _battleEnded = {};
    bool _pendingNextTurn = {};
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

public:
    static Shared<BattleController> Create();
    void Free() override;

public:

};


NS_END
