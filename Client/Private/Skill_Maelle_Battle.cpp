#include "Skill_Maelle_Battle.h"
#include "BattleMechanismCom.h"
#include "DamageSystem.h"
#include "ClientInstance.h"
#include "EffectCom.h"

namespace
{
    static void Play_TargetHitEffect(const BattleChar& target, const BattleHitSequenceRunner::HitEffectDesc& hitEffect)
    {
        if (!hitEffect.Is_Valid() || target.actor == nullptr)
            return;

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

    static void Play_TargetHitSound(const BattleChar& target, const wstring& hitSoundCue)
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
}

/* 공격 태세 */
void Skill_Maelle_OffensiveSwitch::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Offensive);
}

Shared<Skill_Maelle_OffensiveSwitch> Skill_Maelle_OffensiveSwitch::Create()
{
    return make_shared<Skill_Maelle_OffensiveSwitch>();
}

/* 돌파 */
void Skill_Maelle_Percee::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    const bool wasVirtuoseStance =
        attacker.mechanismCom != nullptr &&
        attacker.mechanismCom->Get_MaelleStance() == BattleMechanismCom::MaelleStance::Virtuose;

    const bool hadMark = battle->Has_Buff(target, Buffs::Mark);

    float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);

    if (hadMark)
    {
        constexpr float kMarkedPerceeDamageMultiplier = 1.5f;
        damage *= kMarkedPerceeDamageMultiplier;
    }

    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    if (wasVirtuoseStance && attacker.statCom != nullptr)
    {
        constexpr uint32 kVirtuosePerceeApGain = 2;
        attacker.statCom->Add_AP(kVirtuosePerceeApGain);
    }

    if (target.actor != nullptr)
    {
        BattleHitSequenceRunner::HitEffectDesc effectDesc{};
        effectDesc.effectName = L"Maelle_Procee";
        effectDesc.localOffset = Vec3(0.f, 0.f, 0.f);

        Play_TargetHitEffect(target, effectDesc);
    }

    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Offensive);
}

Shared<Skill_Maelle_Percee> Skill_Maelle_Percee::Create()
{
    return make_shared<Skill_Maelle_Percee>();
}

/* 빠른 걸음 */
void Skill_Maelle_FireRise::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Virtuose);
}

Shared<Skill_Maelle_FireRise> Skill_Maelle_FireRise::Create()
{
    return make_shared<Skill_Maelle_FireRise>();
}

/* 플뢰레 격노 */
void Skill_Maelle_FleureFury::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = (DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier)) / 3.f;
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    battle->Try_ApplySkillBuff(skill, target);

    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Virtuose);
}

Shared<Skill_Maelle_FleureFury> Skill_Maelle_FleureFury::Create()
{
    return make_shared<Skill_Maelle_FleureFury>();
}

/* 불꽃 */
void Skill_Maelle_Spark::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    // 공격태세면 연소 2스택 추가
    const bool wasOffensiveStance =
        attacker.mechanismCom != nullptr &&
        attacker.mechanismCom->Get_MaelleStance() == BattleMechanismCom::MaelleStance::Offensive;

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    constexpr uint32 kSparkBurnStack = 3;
    constexpr uint32 kOffensiveBonusBurnStack = 2;

    battle->Apply_Buff(target, Buffs::Burn, kSparkBurnStack, skill.buffDurationTurn);

    if (wasOffensiveStance)
    {
        battle->Apply_Buff(target, Buffs::Burn, kOffensiveBonusBurnStack, skill.buffDurationTurn);
    }

    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Defensive);
}

Shared<Skill_Maelle_Spark> Skill_Maelle_Spark::Create()
{
    return make_shared<Skill_Maelle_Spark>();
}

/* 스탕달 */
void Skill_Maelle_Stendhal::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    // 임시로 명인태세
    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Virtuose);
}

Shared<Skill_Maelle_Stendhal> Skill_Maelle_Stendhal::Create()
{
    return make_shared<Skill_Maelle_Stendhal>();
}

/* 검의 발레 */
void Skill_Maelle_SwordBallet::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier) / 4.f;

    const DamageAppliedInfo damageInfo =
        DamageSystem::ApplyDamage(damage, attacker, target, attackType, nullptr, 2.f);

    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Offensive);
}

Shared<Skill_Maelle_SwordBallet> Skill_Maelle_SwordBallet::Create()
{
    return make_shared<Skill_Maelle_SwordBallet>();
}

/* 열분해 */
void Skill_Maelle_Pyrolyse::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    const bool wasOffensiveStance =
        attacker.mechanismCom != nullptr &&
        attacker.mechanismCom->Get_MaelleStance() == BattleMechanismCom::MaelleStance::Offensive;

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    constexpr uint32 kPyrolyseBurnStack = 5;
    constexpr uint32 kOffensiveBonusBurnStack = 2;

    battle->Apply_Buff(target, Buffs::Burn, kPyrolyseBurnStack, skill.buffDurationTurn);

    if (wasOffensiveStance)
    {
        battle->Apply_Buff(target, Buffs::Burn, kOffensiveBonusBurnStack, skill.buffDurationTurn);
    }

    if (attacker.mechanismCom != nullptr)
        attacker.mechanismCom->Set_MaelleStance(BattleMechanismCom::MaelleStance::Virtuose);
}

Shared<Skill_Maelle_Pyrolyse> Skill_Maelle_Pyrolyse::Create()
{
    return make_shared<Skill_Maelle_Pyrolyse>();
}
