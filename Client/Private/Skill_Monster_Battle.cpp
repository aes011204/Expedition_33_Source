#include "pch.h"
#include "Skill_Monster_Battle.h"

#include "BattleHitSequenceRunner.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"
#include "DamageSystem.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "Monster_BattleBase.h"

NS_BEGIN(Client)

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
}

Base_Mon_Attack::Base_Mon_Attack()
{
}

void Base_Mon_Attack::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    UNREFERENCED_PARAMETER(attackType);

    //const float Damage = 50;

    if (CurIndex >= skill.targetingType.size())
    {
        LOG_ERROR("ERROR_Base_Mon_Attack_ OutofRange");
        return;
    }
    const BattleChar& attacker = action.Get_CurChar();
    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);

    DamageSystem::ApplyDamage(damage, action.GetCurStepTarget(), attackType);

    CurIndex++;
}

Shared<Base_Mon_Attack> Base_Mon_Attack::Create()
{
    return make_shared<Base_Mon_Attack>();
}

void Base_Mon_Attack::Free()
{
    SkillScript::Free();
}

Base_Mon_Attack_Projectile::Base_Mon_Attack_Projectile()
{
}

void Base_Mon_Attack_Projectile::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    UNREFERENCED_PARAMETER(attackType);

    //const float Damage = 50;

    if (CurIndex >= skill.targetingType.size())
    {
        LOG_ERROR("ERROR_Base_Mon_Attack_Projectile_ OutofRange");
        return;
    }
    const BattleChar& attacker = action.Get_CurChar();
    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);

    DamageSystem::ApplyDamage(damage, action.GetCurStepTarget(), attackType);

    const auto& curTargets = action.GetCurStepTarget();
    if (!curTargets.empty() && curTargets.front().actor != nullptr)
    {
        BattleHitSequenceRunner::HitEffectDesc effectDesc{};
        effectDesc.effectName = L"Bishop_Projectile_Attack";
        effectDesc.localOffset = Vec3(0.f, 0.f, 0.f);

        Internal::Play_TargetHitEffect(curTargets.front(), effectDesc);
    }

    CurIndex++;
}

Shared<Base_Mon_Attack_Projectile> Base_Mon_Attack_Projectile::Create()
{
    return make_shared<Base_Mon_Attack_Projectile>();
}

void Base_Mon_Attack_Projectile::Free()
{
    SkillScript::Free();
}

Bishop_Spear::Bishop_Spear()
{
}

void Bishop_Spear::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    UNREFERENCED_PARAMETER(attackType);

    if (CurIndex >= skill.targetingType.size())
    {
        LOG_ERROR("ERROR_Bishop_Spear_ OutofRange");
        return;
    }

    const BattleChar& attacker = action.Get_CurChar();
    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);

    DamageSystem::ApplyDamage(damage, action.GetCurStepTarget(), attackType);

    const auto& curTargets = action.GetCurStepTarget();
    if (!curTargets.empty() && curTargets.front().actor != nullptr)
    {
        BattleHitSequenceRunner::HitEffectDesc effectDesc{};
        effectDesc.effectName = L"Bishop_Spear_Attack";
        effectDesc.localOffset = Vec3(0.f, 0.f, 0.f);

        Internal::Play_TargetHitEffect(curTargets.front(), effectDesc);
    }


    CurIndex++;
}

Shared<Bishop_Spear> Bishop_Spear::Create()
{
    return make_shared<Bishop_Spear>();
}

void Bishop_Spear::Free()
{
    SkillScript::Free();
}

Base_Mon_Defense::Base_Mon_Defense()
{
}

void Base_Mon_Defense::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    UNREFERENCED_PARAMETER(skill);
    UNREFERENCED_PARAMETER(attackType);

    const uint32 shieldNum = { 5 };

    if (action.Get_CurChar().statCom)
        action.Get_CurChar().statCom->Add_Shield(shieldNum);
}

Shared<Base_Mon_Defense> Base_Mon_Defense::Create()
{
    return make_shared<Base_Mon_Defense>();
}

void Base_Mon_Defense::Free()
{
    SkillScript::Free();
}

Base_Mon_Spawn::Base_Mon_Spawn()
{
}

void Base_Mon_Spawn::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    UNREFERENCED_PARAMETER(skill);
    UNREFERENCED_PARAMETER(action);
    UNREFERENCED_PARAMETER(attackType);

    const auto allEnemy = CLIENT->Get_BattleController()->Get_AllEnemies();
    const auto aliveEnemy = CLIENT->Get_BattleController()->Get_AliveEnemies();
    if (allEnemy.size() == 3)
    {
        //
        for (auto& it : allEnemy)
        {
            if (it.statCom->Get_CurAliveState() == EAliveStatus::Dead)
                CLIENT->Get_BattleController()->Revive_BattleMon(it, 1.f);
        }
    }
    else if (allEnemy.size() == 1)
    {
        const Shared<GameObject> summoner = action.Get_CurChar().actor;
        if (summoner == nullptr || summoner->Get_Transform() == nullptr)
            return;

        const Shared<TransformCom> summonerTransform = summoner->Get_Transform();
        const Vec3 right = summonerTransform->Get_WorldRight();
        const Vec3 pos = summonerTransform->Get_Position();
        const float yawDeg = summonerTransform->Get_RotationEuler().y;

        CLIENT->Get_BattleController()->Spawn_BattleChar(false, "MB_Abbest", pos + right * 4.f, yawDeg);
        CLIENT->Get_BattleController()->Spawn_BattleChar(false, "MB_Abbest", pos - right * 4.f, yawDeg);
    }
}

Shared<Base_Mon_Spawn> Base_Mon_Spawn::Create()
{
    return make_shared<Base_Mon_Spawn>();
}

void Base_Mon_Spawn::Free()
{
    SkillScript::Free();
}

Simon_Combo::Simon_Combo()
{
}

void Simon_Combo::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    UNREFERENCED_PARAMETER(attackType);

    //const float Damage = 50;

    if (CurIndex >= skill.targetingType.size())
    {
        LOG_ERROR("ERROR_Simon_Combo_ OutofRange");
        return;
    }
    const BattleChar& attacker = action.Get_CurChar();
    const float damage = DamageSystem::CalculatePlayerAttackDamage(attacker, skill.powerMultiplier);

    DamageSystem::ApplyDamage(damage, action.GetCurStepTarget(), attackType);

    CurIndex++;
}

Shared<Simon_Combo> Simon_Combo::Create()
{
    return make_shared<Simon_Combo>();
}

void Simon_Combo::Free()
{
    SkillScript::Free();
}

Simon_Debuff::Simon_Debuff()
{
}

void Simon_Debuff::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    UNREFERENCED_PARAMETER(skill);
    UNREFERENCED_PARAMETER(attackType);

    for (const BattleChar& target : action.GetCurStepTarget())
    {
        if (target.statCom == nullptr)
            continue;

        if (target.statCom->Get_AliveStatus() != EAliveStatus::Dead)
            continue;

        target.statCom->Set_ReviveBlocked(true);
    }

    CurIndex++;
}

Shared<Simon_Debuff> Simon_Debuff::Create()
{
    return make_shared<Simon_Debuff>();
}

void Simon_Debuff::Free()
{
    SkillScript::Free();
}

NS_END
