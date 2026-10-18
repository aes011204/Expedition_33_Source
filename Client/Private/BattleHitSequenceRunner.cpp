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

    if (!Has_ValidTarget(_pending.targets))
    {
        Clear();
        return;
    }

    _pending.elapsed += timeDelta;

    while (_pending.elapsed >= _pending.hitInterval && _pending.remainingHits > 0)
    {
        _pending.elapsed -= _pending.hitInterval;

        _isApplyingHitSequenceDamage = true;
        _isCurrentHitSequenceDamageFinal = _pending.remainingHits == 1; // 지연 다단히트는 마지막 예약 타격에서만 파괴를 허용한다.

        for (const BattleChar& target : _pending.targets)
        {
            Apply_SequenceHit(target);
        }

        _isApplyingHitSequenceDamage = false;
        _isCurrentHitSequenceDamageFinal = false;

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
    Begin_RepeatedHit(
        attacker,
        vector<BattleChar>{ target },
        hitCount,
        interval,
        hitDamage,
        attackType,
        hitEffect);
}

void BattleHitSequenceRunner::Begin_RepeatedHit(
    const BattleChar& attacker,
    const vector<BattleChar>& targets,
    int32 hitCount,
    float interval,
    float hitDamage,
    AttackType attackType,
    const HitEffectDesc& hitEffect,
    bool useTargetFeedbackWorldPos)
{
    Clear();

    if (!Has_ValidTarget(targets))
        return;

    if (hitCount <= 0)
        return;

    if (interval <= 0.f)
        return;

    if (hitDamage <= 0.f)
        return;

    _pending.active = true;
    _pending.attacker = attacker;
    _pending.target = targets.front();
    _pending.targets = targets;
    _pending.remainingHits = hitCount;
    _pending.hitInterval = interval;
    _pending.elapsed = 0.f;
    _pending.hitDamage = hitDamage;
    _pending.attackType = attackType;
    _pending.hitEffect = hitEffect;
    _pending.useTargetFeedbackWorldPos = useTargetFeedbackWorldPos;
}

void BattleHitSequenceRunner::Begin_RepeatedHitWithImmediateFirstHit(
    const BattleChar& attacker,
    const BattleChar& target,
    int32 hitCount,
    float interval,
    float hitDamage,
    AttackType attackType,
    const HitEffectDesc& hitEffect)
{
    Begin_RepeatedHitWithImmediateFirstHit(
        attacker,
        vector<BattleChar>{ target },
        hitCount,
        interval,
        hitDamage,
        attackType,
        hitEffect);
}

void BattleHitSequenceRunner::Begin_RepeatedHitWithImmediateFirstHit(
    const BattleChar& attacker,
    const vector<BattleChar>& targets,
    int32 hitCount,
    float interval,
    float hitDamage,
    AttackType attackType,
    const HitEffectDesc& hitEffect,
    bool useTargetFeedbackWorldPos)
{
    Clear();

    if (!Has_ValidTarget(targets))
        return;

    if (hitCount <= 0)
        return;

    if (interval <= 0.f)
        return;

    if (hitDamage <= 0.f)
        return;

    _isApplyingHitSequenceDamage = true;
    _isCurrentHitSequenceDamageFinal = hitCount == 1; // 즉시 첫 타도 다단히트 전체 기준 마지막 여부로 판단한다.

    _pending.attacker = attacker;
    _pending.targets = targets;
    _pending.hitDamage = hitDamage;
    _pending.attackType = attackType;
    _pending.hitEffect = hitEffect;
    _pending.useTargetFeedbackWorldPos = useTargetFeedbackWorldPos;

    for (const BattleChar& target : targets)
    {
        Apply_SequenceHit(target);
    }

    _isApplyingHitSequenceDamage = false;
    _isCurrentHitSequenceDamageFinal = false;

    const int32 remainingHits = hitCount - 1;
    if (remainingHits <= 0)
        return;

    Begin_RepeatedHit(
        attacker,
        targets,
        remainingHits,
        interval,
        hitDamage,
        attackType,
        hitEffect,
        useTargetFeedbackWorldPos);
}

void BattleHitSequenceRunner::Clear()
{
    _pending = {};
    _isApplyingHitSequenceDamage = false;
    _isCurrentHitSequenceDamageFinal = false;
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

void BattleHitSequenceRunner::Play_HitSound(const BattleChar& target, const wstring& hitSoundCue) const
{
    if (hitSoundCue.empty())
        return;

    Vec3 soundPosition = Vec3::Zero;
    const Vec3* soundPositionPtr = nullptr;

    if (target.actor != nullptr && target.actor->Get_Transform() != nullptr)
    {
        soundPosition = target.actor->Get_Transform()->Get_WorldPosition();
        soundPositionPtr = &soundPosition;
    }

    GAME->Play_SoundCue(hitSoundCue, soundPositionPtr);
}

bool BattleHitSequenceRunner::Has_ValidTarget(const vector<BattleChar>& targets) const
{
    for (const BattleChar& target : targets)
    {
        if (target.actor != nullptr && target.statCom != nullptr)
            return true;
    }

    return false;
}

void BattleHitSequenceRunner::Apply_SequenceHit(const BattleChar& target)
{
    if (target.actor == nullptr || target.statCom == nullptr)
        return;

    Vec3 feedbackWorldPos{};
    const Vec3* feedbackWorldPosPtr = nullptr;
    if (_pending.useTargetFeedbackWorldPos &&
        target.actor->Try_GetDamageFeedbackWorldPosition(feedbackWorldPos))
    {
        feedbackWorldPosPtr = &feedbackWorldPos;
    }

    DamageSystem::ApplyDamage(
        _pending.hitDamage,
        _pending.attacker,
        target,
        _pending.attackType,
        feedbackWorldPosPtr);

    Play_HitEffect(target, _pending.hitEffect);
    Play_HitSound(target, _pending.hitEffect.hitSoundCue);
}
