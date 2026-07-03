#include "Skill_Lune_Battle.h"
#include "BattleMechanismCom.h"
#include "DamageSystem.h"
#include "ClientInstance.h"
#include "EffectCom.h"
#include "GameInstance.h"

namespace Internal
{
    void Play_TargetHitEffect(const BattleChar& target, const BattleHitSequenceRunner::HitEffectDesc& hitEffect)
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

    void Play_TargetHitSound(const BattleChar& target, const wstring& hitSoundCue)
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

    void Add_PrewarmRequest(vector<EffectPrewarmRequest>& outRequests, const wstring& effectName, uint32 targetCount)
    {
        EffectPrewarmRequest request{};
        request.effectName = effectName;
        request.layerTag = L"Layer_Effect";
        request.targetCount = max(1u, targetCount);
        outRequests.push_back(request);
    }
}

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

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    battle->Try_ApplySkillBuff(skill, target);

    if (target.actor != nullptr)
    {
        BattleHitSequenceRunner::HitEffectDesc effectDesc{};
        effectDesc.effectName = L"Lune_Immolation_Hit";
        effectDesc.localOffset = Vec3(0.f, 0.7f, 0.f);

        Internal::Play_TargetHitEffect(target, effectDesc);
    }
}

Shared<Skill_Lune_Immolation> Skill_Lune_Immolation::Create()
{
    return make_shared<Skill_Lune_Immolation>();
}

void Skill_Lune_Immolation::Collect_PrewarmEffects(const Skill_Data& skill, const BattleEffectPrewarmContext& context, vector<EffectPrewarmRequest>& outRequests) const
{
    UNREFERENCED_PARAMETER(skill);
    UNREFERENCED_PARAMETER(context);

    Internal::Add_PrewarmRequest(outRequests, L"Lune_Immolation_Hit", 2);
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

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, target, attackType);

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
    const float totalDamage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    const float hitDamage = totalDamage / static_cast<float>(hitCount);

    BattleHitSequenceRunner::HitEffectDesc effectDesc{};
    effectDesc.effectName = L"Lune_ThunderFall_Attack";
    effectDesc.hitSoundCue = L"Lune_Thunderfall_Hit";
    effectDesc.scale = { 1.3f, 1.3f, 1.3f };

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    battle->Get_HitSequenceRunner().Begin_RepeatedHitWithImmediateFirstHit(
        attacker,
        target,
        hitCount,
        0.3f,
        hitDamage,
        attackType,
        effectDesc
    );
}

Shared<Skill_Lune_Thunderfall> Skill_Lune_Thunderfall::Create()
{
    return make_shared<Skill_Lune_Thunderfall>();
}

void Skill_Lune_Thunderfall::Collect_PrewarmEffects(const Skill_Data& skill, const BattleEffectPrewarmContext& context, vector<EffectPrewarmRequest>& outRequests) const
{
    UNREFERENCED_PARAMETER(context);

    const uint32 hitCount = skill.AttackCount > 0 ? static_cast<uint32>(skill.AttackCount) : 6u;
    Internal::Add_PrewarmRequest(outRequests, L"Lune_ThunderFall_Attack", hitCount + 2u);
}

/* 대지 전율 */
void Skill_Lune_TerraQuake::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    BattleHitSequenceRunner::HitEffectDesc effectDesc{};
    effectDesc.effectName = L"Lune_TerraQuake_Attack";
    effectDesc.scale = { 1.3f, 1.4f, 1.3f };

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, curTargets, attackType, true);

    for (auto& target : curTargets)
    {
        Internal::Play_TargetHitEffect(target, effectDesc);
    }

    // 한 번만. 이거 근데 왜이리 소리가 작아;
    GAME->Play_SoundCue(L"Lune_TerraQuake_Hit", nullptr, 4.5f, 1.f);
}

Shared<Skill_Lune_TerraQuake> Skill_Lune_TerraQuake::Create()
{
    return make_shared<Skill_Lune_TerraQuake>();
}

void Skill_Lune_TerraQuake::Collect_PrewarmEffects(const Skill_Data& skill, const BattleEffectPrewarmContext& context, vector<EffectPrewarmRequest>& outRequests) const
{
    UNREFERENCED_PARAMETER(skill);

    Internal::Add_PrewarmRequest(outRequests, L"Lune_TerraQuake_Attack", max(2u, context.currentEnemyCount));
}

/* 억압의 쓰나미 */
void Skill_Lune_CripplingTsunami::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, curTargets, attackType, true);

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    for (const BattleChar& target : curTargets)
    {
        battle->Try_ApplySkillBuff(skill, target);
    }
}

Shared<Skill_Lune_CripplingTsunami> Skill_Lune_CripplingTsunami::Create()
{
    return make_shared<Skill_Lune_CripplingTsunami>();
}

/* 하늘 파괴 */
void Skill_Lune_SkyBreak::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    if (attacker.mechanismCom)
        attacker.mechanismCom->Add_LuneStains({ ElementalType::Ice, ElementalType::Light });

    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);
    DamageSystem::ApplyDamage(damage, attacker, curTargets, attackType, true);
}

Shared<Skill_Lune_SkyBreak> Skill_Lune_SkyBreak::Create()
{
    return make_shared<Skill_Lune_SkyBreak>();
}

