#pragma once
#include "Base.h"
#include "Battle_Enum.h"
NS_BEGIN(Engine)

    class Character;

NS_END


NS_BEGIN(Client)


class BattleStateMachine;
class BattleController :
    public Base
{

public:
    struct Action
    {
        EBattleContext type={};
        Weak<Character> _curChar = {};
        //ActonType _CurActionType = {};
        // Item_Data   _selected_Item
        // Skill_Data   _selected_Skill
        uint32  target = {};
    };
    struct TurnEntry
    {
        Weak<class Character> hero;
        float nextTime;
    };

public:
    HRESULT Inintialize();
    void Update(float timeDelta);



    void Run_EnemyTurn();


    void  OnEnterBattleField(uint32 FieldID, bool IsFirstHero, vector<string>& protoTag);
    EBattleEndResult OnExitBattleField(EBattleEndResult result);

    void Set_BattleFSM();
    void  ExecuteAction(Action turnAction); //실제로 선택한 행동을 실행하는 함수

    void EndTurn();
    void StartTurn();

    EAPChangeReason FinishBattle();



    void Create_BattleChar(vector<string>& protoTag);
    void Build_InitialTurnQueue(bool IsFirstHero);
    void SetCharInitPos();

public:
    Shared< BattleStateMachine>  Get_BattleFSM() { return _battleFSM; };
    Action& Get_TurnAction() { return _turnAction; }
    uint32 Get_EnemiesNum() { return _enemies.size(); }
    uint32 Get_HeroesNum() { return _heroes.size(); }
private:
    Weak<Character> _currentTurnActor = {};
    Action _turnAction={};
    bool isBattleActive = { false };
    int currentTurnIndex = {};
     


private:
    void Execute_BaseAttack(const Shared<Character>& actor, const Action& turnAction);
    void Execute_Skill(const Shared<Character>& actor, const Action& turnAction);
    void Execute_Item(const Shared<Character>& actor, const Action& turnAction);




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
    vector<Shared<Character>> _heroes;
    vector<Shared<Character>> _enemies;
    Shared< BattleStateMachine> _battleFSM;
};


NS_END
