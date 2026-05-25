#include "BattleReactionController.h"

#include "CombatStatCom.h"
#include "DamageSystem.h"
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
}

void BattleReactionController::Clear_RuntimeState()
{
    Clear();

    if (_reactionAnimTarget.ModelCom)
        _reactionAnimTarget.ModelCom->Set_FrameSpeed(1.f);

    _reactionAnimTarget = {};
    _counterDamageTarget = {};
    _animPhase = ReactionAnimPhase::None;
    _isCounterPending = false;
    _isCounterDamageApplied = false;
}

bool BattleReactionController::TryInput(BattleReactionType type)
{
    if (_isConsumed)
        return false;

    if (!IsWindowOpen(type))
        return false;

    _isConsumed = true;
    _result = type;

    return true;
}

bool BattleReactionController::TryConsume(BattleReactionType& outResult)
{
    if (!_isConsumed)
        return false;

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

BattleReactionController::BattleReactionInputResult BattleReactionController::UpdateInput(const Action& turnAction)
{
    UpdateAnimation();

    BattleReactionInputResult result{};
    const BattleChar& reactionTarget = turnAction.target;  // 회피/패링 애니메이션을 재생할 플레이어
    const BattleChar& counterTarget = turnAction._curChar; // Parry 성공 후 Counter 데미지를 받을 현재 공격자

    // 입력받지 못할 떄 
    if (!CanReceiveInput(reactionTarget))
        return result;

    // 이미 소비 했으면
    if (IsConsumed())
        return result;

    // 회피
    if (GAME->KeyDown(KEY_TYPE::Q))
    {
        result.hasInput = true;
        result.type = BattleReactionType::Dodge;
        result.success = TryInput(BattleReactionType::Dodge);

        // 실패했을 때도 애니메이션은 재생
        PlayReactionAnimation(reactionTarget, BattleReactionType::Dodge, result.success, {});
        return result;
    }
    // 패링
    if (GAME->KeyDown(KEY_TYPE::E))
    {
        result.hasInput = true;
        result.type = BattleReactionType::Parry;
        result.success = TryInput(BattleReactionType::Parry);

        // 성공하면 Counter
        PlayReactionAnimation(reactionTarget, BattleReactionType::Parry, result.success, counterTarget);
        return result;
    }

    return result;
}

void BattleReactionController::UpdateAnimation()
{
    if (_reactionAnimTarget.ModelCom == nullptr)
        return;

    if (!_reactionAnimTarget.ModelCom->Is_AnimationFinished())
        return;

    FinishReactionAnimation();
}

void BattleReactionController::StartCounter(ModelCom* sourceModel, const BattleSlowDesc& slowDesc, float counterPlayRate)
{
    if (!_isCounterPending)
        return;

    if (_reactionAnimTarget.ModelCom == nullptr || _reactionAnimTarget.ModelCom.get() != sourceModel)
        return;

    if (!_reactionAnimTarget.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Counter), 0.12f, false))
        return;

    GAME->Slow_WorldTime(slowDesc.worldTimeScale, slowDesc.duration);
    _reactionAnimTarget.ModelCom->Set_FrameSpeed(counterPlayRate);

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

    if (_reactionAnimTarget.ModelCom == nullptr || _reactionAnimTarget.ModelCom.get() != sourceModel)
        return; 

    if (_counterDamageTarget.actor == nullptr || _counterDamageTarget.statCom == nullptr)
        return;

    constexpr float counterDamage = 50.f; // 임시 고정값.
    DamageSystem::ApplyDamage(counterDamage, _counterDamageTarget);

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
    bool isSuccess, const BattleChar& counterTarget)
{
    if (target.ModelCom == nullptr)
        return;

    BattleAnimType animType = BattleAnimType::Base_Idle;

    if (reactionType == BattleReactionType::Dodge)
    {
        animType = BattleAnimType::Dodge;
    }
    else if (reactionType == BattleReactionType::Parry)
    {
        animType = BattleAnimType::Parray; // 성공하면 패링 애니메이션

        if (isSuccess)
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

    if (!target.ModelCom->Set_AnimationByState(ETOI(animType), 0.1f, false))
        return;

    _reactionAnimTarget = target;
    _animPhase = ReactionAnimPhase::Reaction;
}

void BattleReactionController::FinishReactionAnimation()
{
    if (_reactionAnimTarget.ModelCom)
    {
         // Counter 배속이 다음 애니메이션으로 새지 않게 복구
        _reactionAnimTarget.ModelCom->Set_FrameSpeed(1.f);

        _reactionAnimTarget.ModelCom->Set_AnimationByState(
            ETOI(BattleAnimType::Base_Idle),
            0.15f,
            true);
    }

    _reactionAnimTarget = {};
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
