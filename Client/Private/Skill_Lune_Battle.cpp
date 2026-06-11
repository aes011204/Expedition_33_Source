#include "Skill_Lune_Battle.h"
#include "BattleMechanismCom.h"
#include "DamageSystem.h"
#include "ClientInstance.h"

/* 제물 */
void Skill_Lune_Immolation::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& attacker = action.Get_CurChar();
    const BattleChar& target = curTargets.front();

    if (attacker.mechanismCom)
        attacker.mechanismCom->Add_LuneStains({ ElementalType::Fire });

    DamageSystem::ApplyDamage(15.f, attacker, target, attackType);

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    battle->Try_ApplySkillBuff(skill, target);

    // TEMP. 이거 일단 임시. 프로토 타입 이후 제대로 구조잡기. 근데 그냥 이렇게 코드로 제어해도 될거같기도 하고
    if (target.actor != nullptr)
    {
        PlayEffectDesc effectDesc{};
        effectDesc.effectName = L"Effects\\Assets\\Player\\Lune\\Lune_Immolation_Hit.effect.json";
        effectDesc.spawnBasisTarget = target.actor;
        effectDesc.localOffset = Vec3(0.f, 0.7f, 0.f);

        CLIENT->Play_Effect(effectDesc);
    }
}

Shared<Skill_Lune_Immolation> Skill_Lune_Immolation::Create()
{
    return make_shared<Skill_Lune_Immolation>();
}

/* 얼음 창 */
void Skill_Lune_IceGust::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& attacker = action.Get_CurChar();
    const BattleChar& target = curTargets.front();

    if (attacker.mechanismCom)
        attacker.mechanismCom->Add_LuneStains({ ElementalType::Ice, ElementalType::Light });

    DamageSystem::ApplyDamage(15.f, attacker, target, attackType);

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    battle->Try_ApplySkillBuff(skill, target);
}

Shared<Skill_Lune_IceGust> Skill_Lune_IceGust::Create()
{
    return make_shared<Skill_Lune_IceGust>();
}

/* 천둥 내림 */
void Skill_Lune_Thunderfall::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& attacker = action.Get_CurChar();
    const BattleChar& target = curTargets.front();

    if (attacker.mechanismCom)
        attacker.mechanismCom->Add_LuneStains({ ElementalType::Lightning, ElementalType::Light });

    const int32 hitCount = skill.AttackCount > 0 ? skill.AttackCount : 6;
    const float totalDamage = 80.f;
    const float hitDamage = totalDamage / static_cast<float>(hitCount);

    DamageSystem::ApplyDamage(hitDamage, attacker, target, attackType); // 첫 타 호출 시점은 AN_Attack

    const int32 remainingHits = hitCount - 1; // hitCount에서 한대 때렸으니까 -1
    if (remainingHits <= 0)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    battle->Get_HitSequenceRunner().Begin_RepeatedHit(
        attacker,
        target,
        remainingHits, // 이미 첫 타를 넣었으므로 남은 5타만 예약한다.
        0.3f,
        hitDamage,
        attackType);
}

Shared<Skill_Lune_Thunderfall> Skill_Lune_Thunderfall::Create()
{
    return make_shared<Skill_Lune_Thunderfall>();
}
