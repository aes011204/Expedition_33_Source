#include "AN_Attack.h"

#include "BattleController.h"
#include "ClientInstance.h"
#include "SkillScript.h"

void AN_Attack::Execute(const AnimNotifyContext& context)
{
   if(context.isPreview)
       return;

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    if (battle->Get_CurrentScript() == nullptr)
        return;

    // Free Aim은 스킵
    if (battle->Is_FreeAimShotPlaying())
        return;

    BattleAction& turnAction = battle->Get_TurnAction();
    if (battle->Get_AliveEnemies().empty()|| battle->Get_AliveHeroes().empty())
         return;

    const auto& curTargets = turnAction.GetCurStepTarget();
    const BattleChar reactionTarget =
        !curTargets.empty() ? curTargets.front() : turnAction.GetInitTarget();

    // 회피/패링 성공 입력이 있으면 이번 공격 notify의 데미지를 소비하고 스킵한다.
    const bool guarded = battle->Try_ConsumeReactionGuard(reactionTarget);

    AttackType attackType = ResolveAttackType(turnAction, turnAction.GetCurStepIndex());
    if (!guarded)
    {
        battle->Get_CurrentScript()->Execute(
            turnAction.GetSkillData(),
            turnAction,
            attackType);
    }

    // 어택 한번 한거다 라는 표시
    turnAction.Spend_AttackStep();
}

AttackType AN_Attack::ResolveAttackType(const BattleAction& action, uint32 index)
{
    if (action.GetSkillData().targetingType.size() == 1)
    {
        if (action.GetSkillData().targetingType[index] == TargetType::AllEnemies)
        {
            return  AttackType::SingleAOE;
        }
        return AttackType::BaseAttack;
    }
    else if (index == action.GetSkillData().targetingType.size() - 1)
        return AttackType::ComboLast;
    else
    {
        return AttackType::Combo;
    }

}
