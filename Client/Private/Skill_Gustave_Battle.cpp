#include "Skill_Gustave_Battle.h"
#include "BattleMechanismCom.h"
#include "DamageSystem.h"
#include "ClientInstance.h"
#include "EffectCom.h"

namespace Internal
{
struct TargetEffectDesc
{
    wstring effectName{};
    Vec3 localOffset{ Vec3::Zero };
    Vec3 localRotation{ Vec3::Zero };
    Vec3 scale{ Vec3::One };
};

void Play_TargetEffect(
    const BattleChar& target,
    const wstring& effectName,
    const Vec3& localOffset = Vec3::Zero,
    const Vec3& localRotation = Vec3::Zero,
    const Vec3& scale = Vec3::One)
{
    if (effectName.empty() || target.actor == nullptr)
        return;

    const Shared<EffectCom> effectCom = target.actor->Get_Component<EffectCom>();
    if (effectCom == nullptr)
        return;

    EffectCom::EffectPlayDesc effectDesc{};
    effectDesc.levelIndex = GAME->Current_LevelIndex();
    effectDesc.effectName = effectName;
    effectDesc.target = EffectCom::EffectPlayTarget::OwnerRoot;
    effectDesc.localOffset = localOffset;
    effectDesc.localRotation = localRotation;
    effectDesc.scale = scale;
    effectDesc.followTarget = false;

    effectCom->Play_Effect(effectDesc);
}
}

/* 뤼미에르 */
void Skill_Gustave_LumiereAssault::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    if (attacker.mechanismCom)
        attacker.mechanismCom->Add_GustaveCharge(1);
}

Shared<Skill_Gustave_LumiereAssault> Skill_Gustave_LumiereAssault::Create()
{
    return make_shared<Skill_Gustave_LumiereAssault>();
}

/* 과충전 */
void Skill_Gustave_UnleashedCharge::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    // 과충전은 추후에, 게이지 비례한 데미지로 변경 예정
    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    battle->Try_ApplySkillBuff(skill, target);

    if (!attacker.mechanismCom)
        return;

    // 과충전 사용 시, 게이지 전부 소모
    attacker.mechanismCom->Consume_AllGustaveCharge();
}

Shared<Skill_Gustave_UnleashedCharge> Skill_Gustave_UnleashedCharge::Create()
{
    return make_shared<Skill_Gustave_UnleashedCharge>();
}

/* 징표 사격 */
void Skill_Gustave_MarkingShot::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();
    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    battle->Apply_Buff(target, Buffs::Mark);

    if (attacker.mechanismCom)
        attacker.mechanismCom->Add_GustaveCharge(1);
}

Shared<Skill_Gustave_MarkingShot> Skill_Gustave_MarkingShot::Create()
{
    return make_shared<Skill_Gustave_MarkingShot>();
}

/* 산산조각 */
void Skill_Gustave_Shatter::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, curTargets, attackType);
}

Shared<Skill_Gustave_Shatter> Skill_Gustave_Shatter::Create()
{
    return make_shared<Skill_Gustave_Shatter>();
}

/* 완벽한 파괴 */
void Skill_Gustave_PerfectBreak::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    battle->Try_ApplySkillBuff(skill, target);

    if (attacker.mechanismCom)
        attacker.mechanismCom->Add_GustaveCharge(1);
}

Shared<Skill_Gustave_PerfectBreak> Skill_Gustave_PerfectBreak::Create()
{
    return make_shared<Skill_Gustave_PerfectBreak>();
}

void Skill_Gustave_AnglesEye::Collect_PrewarmEffects(const Skill_Data&, const BattleEffectPrewarmContext&, vector<EffectPrewarmRequest>& outRequests) const
{
    EffectPrewarmRequest request{};
    request.effectName = L"Gustave_AngelsEye_Slash";
    request.targetCount = 7;
    outRequests.push_back(request);
}

/* 천사의 눈 */
void Skill_Gustave_AnglesEye::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    // 7연격 스폰 로컬 위치 및 회전 세팅
    const Internal::TargetEffectDesc slashEffects[] =
    {
        { L"Gustave_AngelsEye_Slash", Vec3(-0.5f, 0.7f, -0.8f), Vec3(0.f, 0.f, -30.f), Vec3::One },
        { L"Gustave_AngelsEye_Slash", Vec3(0.5f, 0.7f, -1.2f), Vec3(0.f, 0.f, 50.f), Vec3::One },
        { L"Gustave_AngelsEye_Slash", Vec3(-0.4f, 0.7f, -1.f), Vec3(0.f, 0.f, -20.f), Vec3::One },
        { L"Gustave_AngelsEye_Slash", Vec3(0.3f, 0.7f, -0.6f), Vec3(0.f, 0.f, -7.f), Vec3::One },
        { L"Gustave_AngelsEye_Slash", Vec3(0.f, 0.7f, -0.6f), Vec3(0.f, 0.f, -75.f), Vec3::One },
        { L"Gustave_AngelsEye_Slash", Vec3(0.f, 0.7f, -0.9f), Vec3(0.f, 0.f, 20.f), Vec3::One },
        { L"Gustave_AngelsEye_Slash", Vec3(-1.f, 0.7f, -0.7f), Vec3(0.f, 0.f, 90.f), Vec3::One },
    };

    const uint32 slashEffectCount = static_cast<uint32>(size(slashEffects));
    const uint32 slashEffectIndex = min(action.GetCurStepIndex(), slashEffectCount - 1);
    const Internal::TargetEffectDesc& slashEffect = slashEffects[slashEffectIndex];

    Internal::Play_TargetEffect(
        target,
        slashEffect.effectName,
        slashEffect.localOffset,
        slashEffect.localRotation,
        slashEffect.scale);

    if (attacker.mechanismCom)
        attacker.mechanismCom->Add_GustaveCharge(1);
}

Shared<Skill_Gustave_AnglesEye> Skill_Gustave_AnglesEye::Create()
{
    return make_shared<Skill_Gustave_AnglesEye>();
}
