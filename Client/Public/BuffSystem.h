#pragma once
#include "Base.h"
#include "Battle_Enum.h"
#include "Delegate.h"

NS_BEGIN(Client)

class Buff;
class CombatStatCom;

DECLARE_DELEGATE(FOnBuffApplied, const BattleChar&, Buffs); // 버프 적용
DECLARE_DELEGATE(FOnBuffRemoved, const BattleChar&, Buffs); // 버프 삭제
DECLARE_DELEGATE(FOnBuffConsumed, const BattleChar&, Buffs, float); // 버프 소모

class BuffSystem : public Base
{
public:
    BuffSystem() = default;
    ~BuffSystem() override = default;

public:
    HRESULT Initialize();

    void Apply_Buff(const BattleChar& target, Buffs buffId, uint32 stack = 1, int32 durationTurn = 0);
    void Remove_Buff(const BattleChar& target, Buffs buffId);
    void Clear_Buffs(const BattleChar& target);
    void Clear_All(const vector<BattleChar>& heroes, const vector<BattleChar>& enemies);

    bool Has_Buff(const BattleChar& target, Buffs buffId) const;

    // 징표 데미지 추가
    bool Try_ConsumeMarkOnDamage(const BattleChar& attacker, const BattleChar& target, float& inOutDamage);

    void ProcessTurnStart(const BattleChar& target);
    void ProcessTurnEnd(const BattleChar& target);

    bool Has_TurnStartBurn(const BattleChar& target) const;
    void Apply_BurnTurnStartDamage(const BattleChar& target);

    bool Try_ConsumeStunOnTurnStart(const BattleChar& target);
    float Resolve_TurnScheduleSpeed(const BattleChar& target) const;



private:
    void Handle_DamageCalculating(
        const BattleChar& attacker,
        const BattleChar& target,
        float& damage,
        AttackType attackType);

    Shared<Buff> Find_Buff(const BattleChar& target, Buffs buffId) const;
    void Remove_PendingBuffs(const BattleChar& target);


public:
    FOnBuffApplied  OnBuffApplied;
    FOnBuffRemoved  OnBuffRemoved;
    FOnBuffConsumed OnBuffConsumed;

private:
    static constexpr float MARK_BONUS_DAMAGE = 20.f;      // 징표있는 놈 떄릴 때 추가 데미지
    static constexpr float BURN_MAX_HP_RATE = 0.03f;      // 화상 데미지
    static constexpr float SLOW_SPEED_MULTIPLIER = 0.8f;  // 감속

    FDelegateHandle _damageCalculatingHandle{};

public:
    static Shared<BuffSystem> Create();
    void Free() override;

};

NS_END
