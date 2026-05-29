#include "BattleHitSequenceRunner.h"

#include "DamageSystem.h"

void BattleHitSequenceRunner::Update(float timeDelta)
{
    if (!_pending.active)
        return;

    if (_pending.remainingHits <= 0)
    {
        Clear();
        return;
    }

    if (_pending.target.actor == nullptr || _pending.target.statCom == nullptr)
    {
        Clear();
        return;
    }

    _pending.elapsed += timeDelta;

    while (_pending.elapsed >= _pending.hitInterval && _pending.remainingHits > 0)
    {
        _pending.elapsed -= _pending.hitInterval;

        DamageSystem::ApplyDamage(
            _pending.hitDamage,
            _pending.target,
            _pending.attackType);

        --_pending.remainingHits;
    }

    if (_pending.remainingHits <= 0)
        Clear();
}

void BattleHitSequenceRunner::Begin_RepeatedHit(const BattleChar& attacker, const BattleChar& target, int32 hitCount,
    float interval, float hitDamage, AttackType attackType)
{
    Clear();

    if (target.actor == nullptr || target.statCom == nullptr)
        return;

    if (hitCount <= 0)
        return;

    if (interval <= 0.f)
        return;

    if (hitDamage <= 0.f)
        return;

    _pending.active = true;
    _pending.attacker = attacker;
    _pending.target = target;
    _pending.remainingHits = hitCount;
    _pending.hitInterval = interval;
    _pending.elapsed = 0.f;
    _pending.hitDamage = hitDamage;
    _pending.attackType = attackType;
}

void BattleHitSequenceRunner::Clear()
{
    _pending = {};
}
