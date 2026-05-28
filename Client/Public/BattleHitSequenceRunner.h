#pragma once

#include "Battle_Enum.h"

NS_BEGIN(Client)

// 스크립트에서 다단히트 전달 후 BattleController에서 업데이트
class BattleHitSequenceRunner
{
public:
    struct PendingHitSequence
    {
        bool active = false;

        BattleChar attacker{};
        BattleChar target{};

        int32 remainingHits = 0;   // 앞으로 몇 대 더 때릴지
        float hitInterval = 0.3f;  // 타격 간격
        float elapsed = 0.f;       // 누적 시간
        float hitDamage = 0.f;     // 1타당 데미지

        // 첫 타랑 같은 타입으로 들어가게
        AttackType attackType = AttackType::SingleSkill;
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
        AttackType attackType);

    void Clear();

    
    bool Is_Active() const { return _pending.active; }
    const PendingHitSequence& Get_Pending() const { return _pending; }

private:
    PendingHitSequence _pending{};

};

NS_END
