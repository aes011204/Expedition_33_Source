#pragma once

#include "Battle_Enum.h"
#include <queue>

NS_BEGIN(Client)

class BuffSystem;

class TurnScheduler
{
public:
    void Bind_BuffSystem(const Shared<BuffSystem>& buffSystem);
    void Reset();
    void Build_InitialQueue(const vector<BattleChar>& heroes, const vector<BattleChar>& enemies,
        bool isPlayerAdvantage);
    bool TryPopNextAlive(BattleChar& outActor);
    void Schedule_Next(const BattleChar& actor);
    void Schedule_Next(const BattleChar& actor, float scheduleSpeed);
    void Register_Revived(const BattleChar& revivedActor);
    vector<BattleChar> Preview(const BattleChar& currentActor, uint32 maxCount) const;
    bool Will_NextTurnBePlayer(const BattleChar& currentActor) const;

private:
    struct Compare
    {
        bool operator()(const TurnEntry& a, const TurnEntry& b) const
        {
            if (a.nextTime != b.nextTime)
                return a.nextTime > b.nextTime;

            return a.orderSeq > b.orderSeq;
        }
    };

    float Resolve_ScheduleSpeed(const BattleChar& actor) const;
    float Calculate_NextTime(float fromTime, float scheduleSpeed) const;
    Weak<BuffSystem> _buffSystem;

    void Push_Entry(const BattleChar& actor, float nextTime);

    priority_queue<TurnEntry, vector<TurnEntry>, Compare> _queue;
    float _battleTime = 0.f;
    uint64 _nextEntrySeq = 0;
};

NS_END
