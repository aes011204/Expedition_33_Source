#include "Skill_Lune_Battle.h"
#include "BattleMechanismCom.h"
#include "DamageSystem.h"

/* 제물 */
void Skill_Lune_Immolation::Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target)
{
    if (!target.actor || !target.statCom)
        return;

    DamageSystem::ApplyDamage(15.f, target);
}

Shared<Skill_Lune_Immolation> Skill_Lune_Immolation::Create()
{
    return make_shared<Skill_Lune_Immolation>();
}

/* 얼음 창 */
void Skill_Lune_IceGust::Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target)
{
    if (!target.actor || !target.statCom)
        return;

    DamageSystem::ApplyDamage(15.f, target);
}

Shared<Skill_Lune_IceGust> Skill_Lune_IceGust::Create()
{
    return make_shared<Skill_Lune_IceGust>();
}

/* 천둥 내림 */
void Skill_Lune_Thunderfall::Execute(const Skill_Data& skill, const BattleChar& attacker, const BattleChar& target)
{
    if (!target.actor || !target.statCom)
        return;

    const int32 hitCount = skill.AttackCount > 0 ? skill.AttackCount : 6;
    const float totalDamage = 80.f;
    const float hitDamage = totalDamage / static_cast<float>(hitCount);

    for (int32 i = 0; i < hitCount; ++i)
        DamageSystem::ApplyDamage(hitDamage, target);

}

Shared<Skill_Lune_Thunderfall> Skill_Lune_Thunderfall::Create()
{
    return make_shared<Skill_Lune_Thunderfall>();
}
