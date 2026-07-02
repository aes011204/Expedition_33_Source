#include "AN_Attack.h"

#include "ANS_WeaponEcho.h"
#include "BattleController.h"
#include "ClientInstance.h"
#include "SkillScript.h"
#include "SL_Dualliste.h"
#include "SoulLikeController.h"

void AN_Attack::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic)
        return;

    if (auto soulLike = CLIENT->Get_SoulLikeController())
    {
        if (soulLike->Notify_EstherAttack(context.owner))
            return;

        if (const Shared<SL_Dualliste> dualliste = dynamic_pointer_cast<SL_Dualliste>(soulLike->Get_Monster()))
        {
            if (dualliste->Notify_CloneSkillAttack(context.owner))
                return;
        }
    }

    auto battle = CLIENT->Get_BattleController();
    CHECK_NULL(battle);

    if (battle->Get_CurrentScript() == nullptr)
        return;

    if (battle->Is_FreeAimShotPlaying())
        return;

    BattleAction& turnAction = battle->Get_TurnAction();
    if (battle->Get_AliveEnemies().empty() || battle->Get_AliveHeroes().empty())
        return;

    const uint32 sourceStepIndex = turnAction.GetCurStepIndex();
    if (sourceStepIndex >= turnAction.GetSteps().size())
        return;

    const auto& curTargets = turnAction.GetCurStepTarget();
    const BattleChar reactionTarget =
        !curTargets.empty() ? curTargets.front() : turnAction.GetInitTarget();

    BattleReactionType guardedReaction = BattleReactionType::None;
    const bool guarded =
        battle->Get_ReactionController().TryConsumeGuard(
            reactionTarget,
            sourceStepIndex,
            UINT32_MAX,
            &guardedReaction);

    const AttackType attackType = ResolveAttackType(turnAction, sourceStepIndex);
    if (!guarded)
    {
        battle->Get_CurrentScript()->Execute(
            turnAction.GetSkillData(),
            turnAction,
            attackType);
    }
    else if (guardedReaction == BattleReactionType::Parry)
    {
        battle->Get_CurrentScript()->Play_GuardedHitEffect(turnAction);
    }

    // Echo는 원본 공격의 회피/패링 여부와 별개로 별도 판정으로 소비되어야 한다.
    ANS_WeaponEcho::Queue_EchoAttackSteps(
        context,
        turnAction.Get_CurChar(),
        curTargets,
        reactionTarget,
        sourceStepIndex,
        attackType,
        turnAction.GetSkillData().powerMultiplier);

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
