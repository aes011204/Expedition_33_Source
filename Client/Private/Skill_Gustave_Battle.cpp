#include "Skill_Gustave_Battle.h"
#include "BattleMechanismCom.h"
#include "DamageSystem.h"

// TODO : 프로토타입 이후, 데미지 계산식 제대로

/* 뤼미에르 */
void Skill_Gustave_LumiereAssault::Execute(const Skill_Data& skill, const BattleAction& action, AttackType attackType)
{
    const BattleChar& attacker = action.Get_CurChar();

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    DamageSystem::ApplyDamage(15.f, target, attackType);

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

    DamageSystem::ApplyDamage(40.f, target, attackType);

    if (!attacker.mechanismCom)
        return;

    // 과충전 사용 시, 게이지 소모
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

    const auto& curTargets = action.GetCurStepTarget();
    if (curTargets.empty())
        return;

    const BattleChar& target = curTargets.front();

    DamageSystem::ApplyDamage(25.f, target, attackType);

    if (attacker.mechanismCom)
        attacker.mechanismCom->Add_GustaveCharge(1);
}

Shared<Skill_Gustave_MarkingShot> Skill_Gustave_MarkingShot::Create()
{
    return make_shared<Skill_Gustave_MarkingShot>();
}
