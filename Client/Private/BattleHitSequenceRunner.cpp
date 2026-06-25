#include "BattleHitSequenceRunner.h"

#include "ClientInstance.h"
#include "DamageSystem.h"
#include "Character_BattleBase.h"
#include "EffectCom.h"
#include "GameInstance.h"

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
            _pending.attacker,
            _pending.target,
            _pending.attackType);

        Play_HitEffect(_pending.target, _pending.hitEffect);

        --_pending.remainingHits;
    }

    if (_pending.remainingHits <= 0)
        Clear();
}

void BattleHitSequenceRunner::Begin_RepeatedHit(
    const BattleChar& attacker,
    const BattleChar& target,
    int32 hitCount,
    float interval,
    float hitDamage,
    AttackType attackType,
    const HitEffectDesc& hitEffect)
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
    _pending.hitEffect = hitEffect;
}

void BattleHitSequenceRunner::Clear()
{
    _pending = {};
}

void BattleHitSequenceRunner::Play_HitEffect(const BattleChar& target, const HitEffectDesc& hitEffect) const
{
    if (!hitEffect.Is_Valid() || target.actor == nullptr)
    {
        return;
    }

    const Shared<EffectCom> effectCom = target.actor->Get_Component<EffectCom>();
    if (effectCom == nullptr)
        return;

    EffectCom::EffectPlayDesc effectDesc{};
    effectDesc.levelIndex = GAME->Current_LevelIndex();
    effectDesc.effectName = hitEffect.effectName;
    effectDesc.target = EffectCom::EffectPlayTarget::OwnerRoot;
    effectDesc.localOffset = hitEffect.localOffset;
    effectDesc.localRotation = hitEffect.localRotation;
    effectDesc.scale = hitEffect.scale;
    effectDesc.followTarget = false;

    effectCom->Play_Effect(effectDesc);
}
