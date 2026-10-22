#include "TurnScheduler.h"

#include "CombatStatCom.h"
#include "BuffSystem.h"

NS_BEGIN(Client)

void TurnScheduler::Bind_BuffSystem(const Shared<BuffSystem>& buffSystem)
{
    _buffSystem = buffSystem;
}

float TurnScheduler::Resolve_ScheduleSpeed(const BattleChar& actor) const
{
    if (const auto buffSystem = _buffSystem.lock())
        return buffSystem->Resolve_TurnScheduleSpeed(actor);

    return actor.statCom ? max(actor.statCom->Get_Speed(), 1.f) : 1.f;
}

float TurnScheduler::Calculate_NextTime(float fromTime, float scheduleSpeed) const
{
    return fromTime + 100.f / max(scheduleSpeed, 1.f);
}

void TurnScheduler::Reset()
{
    _battleTime = 0.f;
    _nextEntrySeq = 0;
    while (!_queue.empty())
        _queue.pop();
}

bool TurnScheduler::TryPopNextAlive(BattleChar& outActor)
{
    outActor = {};
    while (!_queue.empty())
    {
        const TurnEntry entry = _queue.top();
        _queue.pop();
        _battleTime = entry.nextTime;

        if (entry.hero.actor == nullptr || entry.hero.statCom == nullptr)
            continue;

        if (entry.hero.statCom->Get_AliveStatus() != EAliveStatus::Alive)
            continue;

        outActor = entry.hero;
        return true;
    }

    return false;
}

void TurnScheduler::Schedule_Next(const BattleChar& actor)
{
    Schedule_Next(actor, Resolve_ScheduleSpeed(actor));
}

void TurnScheduler::Schedule_Next(const BattleChar& actor, float scheduleSpeed)
{
    Push_Entry(actor, Calculate_NextTime(_battleTime, scheduleSpeed));
}

void TurnScheduler::Build_InitialQueue(const vector<BattleChar>& heroes, const vector<BattleChar>& enemies, bool isPlayerAdvantage)
{
    float heroOffset = {};
    float monsterOffset = {};

    // 첨 초기화
    _battleTime = 0;
    _nextEntrySeq = 0;

    while (!_queue.empty())
    {
        _queue.pop();
    }

    // 선제 타격이면 몬스터를 늦추고, Player <-> Monster 충돌 진입이면 플레이어를 낮추기
    if (isPlayerAdvantage)
        monsterOffset = 5.f;
    else
        heroOffset = 5.f;

    for (const auto& hero : heroes)
    {
        if (!hero.actor)
            continue;

        Push_Entry(hero, Calculate_NextTime(heroOffset, Resolve_ScheduleSpeed(hero)));
    }
    for (const auto& enemy : enemies)
    {
        if (!enemy.actor)
            continue;

        Push_Entry(enemy, Calculate_NextTime(monsterOffset, Resolve_ScheduleSpeed(enemy)));
    }
}

void TurnScheduler::Register_Revived(const BattleChar& revivedActor)
{
    if (revivedActor.actor == nullptr || revivedActor.statCom == nullptr)
        return;

    priority_queue<TurnEntry, vector<TurnEntry>, Compare> rebuiltQueue{};

    while (!_queue.empty())
    {
        TurnEntry entry = _queue.top();
        _queue.pop();

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
    revivedEntry.nextTime = Calculate_NextTime(_battleTime, Resolve_ScheduleSpeed(revivedActor));
    revivedEntry.orderSeq = _nextEntrySeq++;

    rebuiltQueue.push(revivedEntry);
    _queue = std::move(rebuiltQueue);
}

void TurnScheduler::Push_Entry(const BattleChar& actor, float nextTime)
{
    TurnEntry entry{};
    entry.hero = actor;
    entry.nextTime = nextTime;
    entry.orderSeq = _nextEntrySeq++;

    _queue.push(entry);
}

vector<BattleChar> TurnScheduler::Preview(const BattleChar& currentActor, uint32 maxCount) const
{
    vector<BattleChar> preview{};
    if (maxCount == 0)
        return preview;

    auto queueCopy = _queue;
    uint64 previewNextTurnEntrySeq = _nextEntrySeq;

    if (currentActor.actor != nullptr && currentActor.statCom != nullptr)
    {
        preview.push_back(currentActor);

        // 현재 턴 actor도 다시 들어와야해서 preview 큐에 다시
        if (currentActor.statCom->Get_AliveStatus() == EAliveStatus::Alive)
        {
            TurnEntry nextEntry{};
            nextEntry.hero = currentActor;
            nextEntry.nextTime =
                Calculate_NextTime(_battleTime, Resolve_ScheduleSpeed(currentActor));
            nextEntry.orderSeq = previewNextTurnEntrySeq++;

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

        entry.nextTime = Calculate_NextTime(entry.nextTime, Resolve_ScheduleSpeed(entry.hero));
        entry.orderSeq = previewNextTurnEntrySeq++;
        queueCopy.push(entry);
    }

    return preview;
}

bool TurnScheduler::Will_NextTurnBePlayer(const BattleChar& currentActor) const
{
    auto queueCopy = _queue;
    uint64 previewNextTurnEntrySeq = _nextEntrySeq;

    if (currentActor.actor != nullptr &&
        currentActor.statCom != nullptr &&
        currentActor.statCom->Get_CurAliveState() == EAliveStatus::Alive)
    {
        TurnEntry nextEntry{};
        nextEntry.hero = currentActor;
        nextEntry.nextTime =
            Calculate_NextTime(_battleTime, Resolve_ScheduleSpeed(currentActor));
        nextEntry.orderSeq = previewNextTurnEntrySeq++;
        queueCopy.push(nextEntry);
    }

    while (!queueCopy.empty())
    {
        const TurnEntry entry = queueCopy.top();
        queueCopy.pop();

        if (entry.hero.actor == nullptr || entry.hero.statCom == nullptr)
            continue;

        if (entry.hero.statCom->Get_CurAliveState() != EAliveStatus::Alive)
            continue;

        return entry.hero.statCom->Is_Player();
    }

    return false;
}

NS_END
