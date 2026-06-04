#pragma once

#include "Base.h"
#include "Delegate.h"

NS_BEGIN(Client)

enum class AttackType;
struct BattleChar;
struct DamageAppliedInfo;

// HP가 실제로 깎인 뒤 UI/전투 후처리 데미지 적용
DECLARE_DELEGATE(FOnDamageApplied, const DamageAppliedInfo&);
// HP를 깎기 직전에 버프/상태 이상 데미지 세팅
DECLARE_DELEGATE(FOnDamageCalculating, const BattleChar&, const BattleChar&, float&, AttackType);
// 힐 적용
DECLARE_DELEGATE(FOnHealApplied, const BattleChar&, float);

class DamageSystem : public Base
{
public:
    DamageSystem() = default;
    virtual ~DamageSystem() = default;
public:
    // 단일 대상 데미지 처리
    static void ApplyDamage(float damage, const BattleChar& target, AttackType attackType);
    // Attacker 포함 단일 대상 처리 / 징표 소비, 추후 버프/상태이상 데미지 보정용
    static void ApplyDamage(float damage, const BattleChar& attacker, const BattleChar& target, AttackType attackType);
    // 다중 대상 처리
    static void ApplyDamage(float damage, const vector<BattleChar>& target, AttackType attackType);
    // Asttacker 포함 다중 대상 처리
    static void ApplyDamage(float damage, const BattleChar& attacker, const vector<BattleChar>& target, AttackType attackType);

    static void Heal(float amount, const BattleChar& target);

    static FOnDamageApplied     OnDamageApplied;
    static FOnDamageCalculating OnDamageCalculating;
    static FOnHealApplied       OnHealApplied;

    virtual void Free();
};


NS_END
