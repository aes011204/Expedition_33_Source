#pragma once
#include "Base.h"
NS_BEGIN(Engine)

class Character;
    class BattleStateMachine;
class BattleController :
    public Base
{

public:
    struct TurnEntry
    {
        class Unit* unit;
        float nextTime;
    };

    public:
        void  OnBattleStart();
        void  OnBattleEnd();
        void  ExecuteAction(Character actor, vector<class Charactor> target); //실제로 선택한 행동을 실행하는 함수
private:
    BattleStateMachine fsm;

public:
    struct Compare
    {
        bool operator(TurnEntry& a, TurnEntry& b)
        {
            if (a.nextTime > b.nextTime)
                return true;
            else
                return false;
        }
    };
private:
    priority_queue<TurnEntry, vector<TurnEntry>, Compare> turnQueue;

    vector<class Charactor> _heros;
    vector<class Charactor> _enemies;

    bool isBattleActive = {false};
    int currentTurnIndex = {};
};

NS_END
