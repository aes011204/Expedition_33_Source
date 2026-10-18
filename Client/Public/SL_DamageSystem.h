#pragma once

#include "Battle_Enum.h"
#include "Base.h"
#include "SL_Types.h"

NS_BEGIN(Client)

class CombatStatCom;

// 쌍검투사 전투용 데미지 시스템. 배틀이랑 많이 달라서 따로 만듦
class SL_DamageSystem : public Base
{
public:
    static float Calculate_PhysicalDamage(const Shared<CombatStatCom>& attackerStat, float multiplier);

    static bool Apply_Damage(
        const Shared<GameObject>& attacker,
        const Shared<CombatStatCom>& attackerStat,
        const Shared<GameObject>& target,
        const Shared<CombatStatCom>& targetStat,
        float multiplier,
        AttackType attackType,
        SL_DamageInfo& outInfo);

    static bool Apply_FixedDamage(
        const Shared<GameObject>& attacker,
        const Shared<GameObject>& target,
        const Shared<CombatStatCom>& targetStat,
        float damage,
        AttackType attackType,
        SL_DamageInfo& outInfo);

};

NS_END
