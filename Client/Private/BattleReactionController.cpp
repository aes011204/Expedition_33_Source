#include "BattleReactionController.h"

#include "CombatStatCom.h"
#include "DamageSystem.h"
#include "ClientInstance.h"
#include "BattleController.h"
#include "GameInstance.h"
#include "ModelCom.h"

void BattleReactionController::OpenWindow(BattleReactionType type)
{
    if (type == BattleReactionType::Dodge)
        _isDodgeOpen = true;
    else if (type == BattleReactionType::Parry)
        _isParryOpen = true;
}

void BattleReactionController::CloseWindow(BattleReactionType type)
{
    if (type == BattleReactionType::Dodge)
        _isDodgeOpen = false;
    else if (type == BattleReactionType::Parry)
        _isParryOpen = false;

    // 성공했을 때 입력은 AN_Attack에서 Consume 소모
}

void BattleReactionController::Clear()
{
    _isDodgeOpen = false;
    _isParryOpen = false;
    _isConsumed = false;
    _result = BattleReactionType::None;
    _consumedStepIndex = UINT32_MAX;
}

void BattleReactionController::Clear_RuntimeState()
{
    Clear();

    for (const BattleChar& target : _reactionAnimTargets)
    {
        if (target.ModelCom)
            target.ModelCom->Set_FrameSpeed(1.f);
    }
    _reactionAnimTargets.clear();
    _counterDamageTarget = {};
    _animPhase = ReactionAnimPhase::None;
    _isCounterPending = false;
    _isCounterDamageApplied = false;
}

bool BattleReactionController::TryInput(BattleReactionType type, uint32 stepIndex)
{
    if (_isConsumed)
        return false;

    if (!IsWindowOpen(type))
        return false;

    _isConsumed = true;
    _result = type;
    _consumedStepIndex = stepIndex;
    return true;
}

bool BattleReactionController::TryConsume(BattleReactionType& outResult, uint32 stepIndex)
{
    if (!_isConsumed)
        return false;

    if (_consumedStepIndex != stepIndex)
    {
        // 저장된 입력이 이미 지난 step용이면 버린다.
        if (_consumedStepIndex < stepIndex)
            Clear();

        return false;
    }

    outResult = _result;
    Clear();

    return outResult == BattleReactionType::Dodge || outResult == BattleReactionType::Parry;
}

bool BattleReactionController::IsWindowOpen(BattleReactionType type) const
{
    switch (type)
    {
    case BattleReactionType::Dodge:
        return _isDodgeOpen;
    case BattleReactionType::Parry:
        return _isParryOpen;
    default:
        return false;
    }
}

BattleReactionController::BattleReactionInputResult BattleReactionController::UpdateInput(const BattleAction& turnAction)
{

    UpdateAnimation();

    BattleReactionInputResult result{};

    if (_curSkillData == nullptr)
        return result;

    if (turnAction.GetSteps().size() <= turnAction.GetCurStepIndex())
        return result;

    const auto& curTargets = turnAction.GetCurStepTarget();
    if (curTargets.empty())
        return result;

    if (IsConsumed())
        return result;

    bool pressedQ = GAME->KeyDown(KEY_TYPE::Q);
    bool pressedE = GAME->KeyDown(KEY_TYPE::E);

    if (!pressedQ && !pressedE)
        return result;

    const BattleChar& counterTarget = turnAction.Get_CurChar();
    const uint32 curStepIndex = turnAction.GetCurStepIndex();

    if (pressedQ)
    {
        const bool success = TryInput(BattleReactionType::Dodge, curStepIndex);

        result.hasInput = true;
        result.type = BattleReactionType::Dodge;
        result.success = success;

        for (const BattleChar& target : curTargets)
        {
            if (!CanReceiveInput(target))
                continue;

            PlayReactionAnimation(
                target,
                BattleReactionType::Dodge,
                success,
                {},
                curStepIndex);
        }
    }
    else if (pressedE)
    {
        const bool success = TryInput(BattleReactionType::Parry, curStepIndex);

        result.hasInput = true;
        result.type = BattleReactionType::Parry;
        result.success = success;

        for (const BattleChar& target : curTargets)
        {
            if (!CanReceiveInput(target))
                continue;

            PlayReactionAnimation(
                target,
                BattleReactionType::Parry,
                success,
                counterTarget,
                curStepIndex);
        }
    }

    /*   if ((pressedQ && IsWindowOpen(BattleReactionType::Dodge)) ||
           (pressedE && IsWindowOpen(BattleReactionType::Parry)))
       {
           _isConsumed = true;
       }*/

    return result;
}

BattleReactionController::BattleReactionInputResult BattleReactionController::UpdateTargetInput(const BattleChar& reactionTarget, const BattleChar& counterTarget, uint32 curAnimIndex,
    bool pressedQ, bool pressedE)
{

    BattleReactionInputResult result{};



    //// 회피
    //if (pressedQ)
    //{
    //    result.hasInput = true;
    //    result.type = BattleReactionType::Dodge;
    //    result.success = TryInput(BattleReactionType::Dodge,); ;

    //    // 실패했을 때도 애니메이션은 재생
    //    PlayReactionAnimation(reactionTarget, BattleReactionType::Dodge, result.success, {}, curAnimIndex);
    //}
    //// 패링
    //if (pressedE)
    //{
    //    result.hasInput = true;
    //    result.type = BattleReactionType::Parry;
    //    result.success = TryInput(BattleReactionType::Parry,); ;

    //    // 성공하면 Counter
    //    PlayReactionAnimation(reactionTarget, BattleReactionType::Parry, result.success, counterTarget, curAnimIndex);
    //}


    return result;
}


void BattleReactionController::UpdateAnimation()
{
    if (_reactionAnimTargets.empty())
        return;

    bool allFinished = true;

    for (const BattleChar& target : _reactionAnimTargets)
    {
        if (target.ModelCom == nullptr)
            continue;

        if (!target.ModelCom->Is_AnimationFinished())
        {
            allFinished = false;
            break;
        }
    }

    if (!allFinished)
        return;

    FinishReactionAnimation();
}

void BattleReactionController::StartCounter(ModelCom* sourceModel, const BattleSlowDesc& slowDesc, float counterPlayRate)
{
    if (!_isCounterPending)
        return;
    bool ownsSource = false;
    for (const BattleChar& target : _reactionAnimTargets)
    {
        //if (target.ModelCom && target.ModelCom.get() == sourceModel)
        //{
         //   ownsSource = true;
            if (!target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Counter), 0.12f, false))
                return;
        //    break;
       // }
    }

    //if (!ownsSource)
    //    return;


    GAME->Slow_WorldTime(slowDesc.worldTimeScale, slowDesc.duration);
    sourceModel->Set_FrameSpeed(counterPlayRate); // 플레이어 속도 증가

    _isCounterPending = false;
    _isCounterDamageApplied = false;
    _animPhase = ReactionAnimPhase::Counter;
}

void BattleReactionController::ApplyCounterDamage(ModelCom* sourceModel)
{
    if (_animPhase != ReactionAnimPhase::Counter)
        return;

    if (_isCounterDamageApplied)
        return;


    if (_counterDamageTarget.actor == nullptr || _counterDamageTarget.statCom == nullptr)
        return;

    constexpr float counterDamage = 50.f;

    uint32 hitCount = 0;

    for (const BattleChar& target : _reactionAnimTargets)
    {
        if (target.actor == nullptr || target.statCom == nullptr)
            continue;

        DamageSystem::ApplyDamage(counterDamage, _counterDamageTarget, AttackType::Counter);
        ++hitCount;
    }

    if (hitCount > 0)
        _isCounterDamageApplied = true;
}

bool BattleReactionController::CanReceiveInput(const BattleChar& target) const
{
    if (target.actor == nullptr || target.statCom == nullptr || target.ModelCom == nullptr)
        return false;

    if (!target.statCom->Is_Player())
        return false;

    const BattleAnimType animType =
        static_cast<BattleAnimType>(target.ModelCom->Get_AnimationStateIndex());

    if (IsHitOrDeadBattleAnim(animType))
        return false; // 피격/사망 애니메이션 중에는 불가능

    return true;
}

bool BattleReactionController::IsHitOrDeadBattleAnim(BattleAnimType animType) const
{
    return animType == BattleAnimType::Hit_Heavy ||
        animType == BattleAnimType::Hit_Medium ||
        animType == BattleAnimType::Hit_Light ||
        animType == BattleAnimType::Dead;
}

void BattleReactionController::PlayReactionAnimation(const BattleChar& target, BattleReactionType reactionType,
    bool isSuccess, const BattleChar& counterTarget, uint32 curAttackIndex)
{

    if (target.ModelCom == nullptr)
        return;

    if (target.statCom->Get_AliveStatus() == EAliveStatus::Dead)
        return;

    BattleAnimType animType = BattleAnimType::Base_Idle;

    if (reactionType == BattleReactionType::Dodge)
    {
        animType = BattleAnimType::Dodge;
    }
    else if (reactionType == BattleReactionType::Parry)
    {
        animType = BattleAnimType::Parray; // 성공하면 패링 애니메이션

        if (isSuccess && _curSkillData->targetingType.size() - 1 == curAttackIndex)
        {
            _isCounterPending = true;
            _counterDamageTarget = counterTarget;
            _isCounterDamageApplied = false;
        }
    }
    else
    {
        return;
    }

    target.ModelCom->Set_RootMotionEnabled(false);

    if (!target.ModelCom->Set_AnimationByState(ETOI(animType), 0.1f, false))
        return;

    _reactionAnimTargets.push_back(target);
    _animPhase = ReactionAnimPhase::Reaction;



}

void BattleReactionController::FinishReactionAnimation()
{
    for (const BattleChar& target : _reactionAnimTargets)
    {
        if (target.ModelCom == nullptr)
            continue;
        if (target.statCom->Get_AliveStatus() == EAliveStatus::Dead)
            continue;

        target.ModelCom->Set_RootMotionEnabled(true);

        target.ModelCom->Set_FrameSpeed(1.f);
        target.ModelCom->Set_AnimationByState(
            ETOI(BattleAnimType::Base_Idle),
            0.15f,
            true);
    }

    _reactionAnimTargets.clear();
    _counterDamageTarget = {};
    _animPhase = ReactionAnimPhase::None;
    _isCounterPending = false;
    _isCounterDamageApplied = false;
}

Shared<BattleReactionController> BattleReactionController::Create()
{
    return make_shared<BattleReactionController>();
}

void BattleReactionController::Free()
{
    Base::Free();
}
