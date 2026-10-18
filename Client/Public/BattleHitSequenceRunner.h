#pragma once

#include "Battle_Enum.h"

NS_BEGIN(Client)

// 스크립트에서 다단히트 전달 후 BattleController에서 업데이트
class BattleHitSequenceRunner
{
public:
    struct HitEffectDesc
    {
        wstring effectName{};
        wstring hitSoundCue{};

        Vec3 localOffset{ Vec3::Zero };
        Vec3 localRotation{ Vec3::Zero };
        Vec3 scale{ Vec3::One };

        bool Is_Valid() const { return !effectName.empty(); }
        bool Has_HitSound() const {return !hitSoundCue.empty(); }
    };

    struct PendingHitSequence
    {
        bool active = false;

        BattleChar attacker{};
        BattleChar target{};
        vector<BattleChar> targets{};

        int32 remainingHits = 0;   // 앞으로 몇 대 더 때릴지
        float hitInterval = 0.3f;  // 타격 간격
        float elapsed = 0.f;       // 누적 시간
        float hitDamage = 0.f;     // 1타당 데미지
        bool useTargetFeedbackWorldPos = false;

        // 첫 타랑 같은 타입으로 들어가게
        AttackType attackType = AttackType::SingleSkill;

        HitEffectDesc hitEffect{};
    };

public:
    BattleHitSequenceRunner() = default;
    ~BattleHitSequenceRunner() = default;

public:
    void Update(float timeDelta);
    void Begin_RepeatedHit(
        const BattleChar& attacker,
        const BattleChar& target,
        int32 hitCount,
        float interval,
        float hitDamage,
        AttackType attackType,
        const HitEffectDesc& hitEffect = {});
    void Begin_RepeatedHit(
        const BattleChar& attacker,
        const vector<BattleChar>& targets,
        int32 hitCount,
        float interval,
        float hitDamage,
        AttackType attackType,
        const HitEffectDesc& hitEffect = {},
        bool useTargetFeedbackWorldPos = false);
    void Begin_RepeatedHitWithImmediateFirstHit(
        const BattleChar& attacker,
        const BattleChar& target,
        int32 hitCount,
        float interval,
        float hitDamage,
        AttackType attackType,
        const HitEffectDesc& hitEffect = {});
    void Begin_RepeatedHitWithImmediateFirstHit(
        const BattleChar& attacker,
        const vector<BattleChar>& targets,
        int32 hitCount,
        float interval,
        float hitDamage,
        AttackType attackType,
        const HitEffectDesc& hitEffect = {},
        bool useTargetFeedbackWorldPos = false);

    void Clear();

    bool Is_Active() const { return _pending.active; }
    const PendingHitSequence& Get_Pending() const { return _pending; }
    bool Is_ApplyingHitSequenceDamage() const { return _isApplyingHitSequenceDamage; }
    bool Is_CurrentHitSequenceDamageFinal() const { return _isCurrentHitSequenceDamageFinal; }

private:
    PendingHitSequence _pending{};
    bool _isApplyingHitSequenceDamage = false;
    bool _isCurrentHitSequenceDamageFinal = false;

private:
    // 예약된 반복 타격이 실제 적용된 직후, 설정된 경우에만 타겟 기준 히트 이펙트를 1회 재생한다.
    void Play_HitEffect(const BattleChar& target, const HitEffectDesc& hitEffect) const;

    void Play_HitSound(const BattleChar& target, const wstring& hitSoundCue) const;
    bool Has_ValidTarget(const vector<BattleChar>& targets) const;
    void Apply_SequenceHit(const BattleChar& target);
};

NS_END
