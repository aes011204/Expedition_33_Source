#include "BattleActionExecutor.h"

#include "BattleAnimutils.h"
#include "Character.h"
#include "Monster_BattleBase.h"
#include "CombatStatCom.h"

void BattleActionExecutor::BeginAction(BattleAction& action)
{
    _isFinished = false;
    action.ResetCurAnimIndex();
    Begin_ActionPhase(action);
}

void BattleActionExecutor::Update(float timeDelta, BattleAction& action)
{
 /*   if (action.phaseReachedGoal)
    {
        FinishCurrentPhaseAtGoal(action);
        return;
    }*/

    //LOG_DEBUG("goal : {},{},{}", action.GetPhaseGoalPos().x, action.GetPhaseGoalPos().y, action.GetPhaseGoalPos().z);
    //LOG_DEBUG("start : {},{},{}", action.GetPhaseStartPos().x, action.GetPhaseStartPos().y, action.GetPhaseStartPos().z);

    if (_isFinished)
        return;


    Update_ActionMovement(timeDelta, action);
}

void BattleActionExecutor::OnActionEndNotify(BattleAction& action)
{
    const auto& phases = action.GetSkillData().AnimPhases;
    if (action.GetCurAnimIndex() >= phases.size() )
        return;

    const BattleActionPhase& phase = phases[action.GetCurAnimIndex()];

    if (phase.movementPolicy == BattleMovementPolicy::RootMotionToTarget )//||
        //phase.movementPolicy == BattleMovementPolicy::MoveToTarget )//||
       // phase.movementPolicy == BattleMovementPolicy::ReturnToOrigin)
        return;

    //if (phase.movementPolicy == BattleMovementPolicy::MoveToTarget ||
    //    phase.movementPolicy == BattleMovementPolicy::ReturnToOrigin)
    //{
    //    action.SnapActorToPhaseGoal();
    //    action.ResetPhasePrevRootRatio();
    //    action.SetPhaseReachedGoal(true);
    //}

    Complete_CurPhase(action);
    //action.phaseReachedGoal = true;
}

void BattleActionExecutor::Complete_CurPhase(BattleAction& action)
{
    const auto& phases = action.GetSkillData().AnimPhases;
    if (action.GetCurAnimIndex() >= phases.size())
        return;

    action.AdvanceCurAnimIndex();

    if (action.GetCurAnimIndex() >= phases.size())
    {
        if (action.Get_CurChar().ModelCom)
        {
            BattleAnimUtils::Play_BattleIdle(action.Get_CurChar(), 0.3f);
            //action._curChar.ModelCom->Set_Animation(action._selected_SkillKey.IdleAnimationKey, true, 0.f);
        }

        _isFinished = true;
        action.SetPhaseReachedGoal(true);
        return;
    }

    Begin_ActionPhase(action);
}

void BattleActionExecutor::Begin_ActionPhase(BattleAction& action)
{
    if (!action.Get_CurChar().actor)
        return;

    //phases 가져옴
    const auto phases = action.GetSkillData().AnimPhases;
    if (phases.size() == 0)
        return;

    const BattleActionPhase& phase = phases[action.GetCurAnimIndex()];

    //phase에 현재 단계 데이터 저장
    action.BeginPhase(
        action.Get_CurChar().actor->Get_Transform()->Get_Position(),
        Resolve_PhaseGoalPosition(action, phase));

    // _testPrevRatio = action.Get_CurChar().ModelCom->Get_RootMotionTranslationRatio();

    if (phase.faceTarget)
        Face_ActionTarget(action, action.GetPhaseGoalPos());

    if (phase.movementPolicy == BattleMovementPolicy::RootMotionToTarget)
        action.Get_CurChar().ModelCom->Set_Animation(phase.animationKey, true);
    else
        action.Get_CurChar().ModelCom->Set_Animation(phase.animationKey, false);
}

Vec3 BattleActionExecutor::Resolve_AnchorPosition(const BattleAction& action, BattleTargetAnchor anchor) const
{
    switch (anchor)
    {
    case BattleTargetAnchor::Self:
        return action.Get_CurChar().actor->Get_Transform()->Get_Position();

    case BattleTargetAnchor::PrimaryTarget:
        if (action.GetInitTarget().actor)
            return action.GetInitTarget().actor->Get_Transform()->Get_Position();
        break;

    case BattleTargetAnchor::TargetCenter: // TODO:: 나중에 팀의 중심 값가져오기
        if (action.GetInitTarget().actor)
            return action.GetInitTarget().actor->Get_Transform()->Get_Position();
        break;
    case BattleTargetAnchor::Origin: // TODO:: 나중에 팀의 중심 값가져오기
            return action.GetOriginPos();
        break;
    default:
        break;
    }

    return action.Get_CurChar().actor->Get_Transform()->Get_Position();
}

Vec3 BattleActionExecutor::Resolve_PhaseGoalPosition(const BattleAction& action, const BattleActionPhase& phase) const
{
    if (!action.Get_CurChar().actor)
        return {};

    const Vec3 actorPos = action.Get_CurChar().actor->Get_Transform()->Get_Position();

    if (phase.movementPolicy == BattleMovementPolicy::ReturnToOrigin)
        return action.GetOriginPos();

    const Vec3 anchorPos = Resolve_AnchorPosition(action, phase.targetAnchor);

    if (!phase.useDistance|| phase.targetAnchor == BattleTargetAnchor::Origin)
        return anchorPos;

    if (phase.targetAnchor == BattleTargetAnchor::Self)
        return actorPos;

    Vec3 dir = anchorPos - actorPos;
    dir.y = 0.f;

    if (dir.LengthSquared() <= 0.0001f)
        return anchorPos;

    dir.Normalize();

    return anchorPos - dir * phase.offsetDistance;
}

void BattleActionExecutor::Face_ActionTarget(BattleAction& action, const Vec3& goalPos)
{
    if (!action.Get_CurChar().actor)
        return;

    const auto transform = action.Get_CurChar().actor->Get_Transform();
    const Vec3 pos = transform->Get_Position();

    Vec3 dir = goalPos - pos;
    dir.y = 0.f;

    if (dir.LengthSquared() <= 0.0001f)
        return;

    dir.Normalize();

    const float yawRad = atan2f(dir.x, dir.z);
    const float yawDeg = XMConvertToDegrees(yawRad);

    Vec3 rot = transform->Get_RotationEuler();
    rot.y = yawDeg;
    transform->Set_RotationEuler(rot);
}

void BattleActionExecutor::Update_ActionMovement(float timeDelta, BattleAction& action)
{
    if (action.HasReachedPhaseGoal())
        return;

    const auto& phases = action.GetSkillData().AnimPhases;
    if (action.GetCurAnimIndex() >= phases.size())
        return;

    const BattleActionPhase& phase = phases[action.GetCurAnimIndex()];
    if (phase.movementPolicy == BattleMovementPolicy::Custom)
        return;

    if (!action.Get_CurChar().actor || !action.Get_CurChar().ModelCom)
        return;

    const auto transform = action.Get_CurChar().actor->Get_Transform();
    const Vec3 currentPos = transform->Get_Position();

    if (phase.faceTarget)
        Face_ActionTarget(action, action.GetPhaseGoalPos());

    switch (phase.movementPolicy)
    {
    case BattleMovementPolicy::None:
        Update_BasicMovement(action, transform, currentPos);
        return;

    case BattleMovementPolicy::RootMotionToTarget:
        Update_RootMotionToTarget(action, transform, currentPos);
        return;

    case BattleMovementPolicy::MoveToTarget:
    case BattleMovementPolicy::ReturnToOrigin:
        Update_RatioMoveTo(action, transform, currentPos);
        return;

    default:
        return;
    }
}

void BattleActionExecutor::Update_BasicMovement(BattleAction& action, const Shared<TransformCom>& transform, const Vec3& currentPos)
{
    //NONE
    const auto& rm = action.Get_CurChar().ModelCom->Get_RootTransformVelocity();
    const Vec3 delta = rm.position * 0.01f;

    const Vec3 rotDeg = transform->Get_RotationEuler();
    const Vec3 rotRad(
        XMConvertToRadians(rotDeg.x),
        XMConvertToRadians(rotDeg.y),
        XMConvertToRadians(rotDeg.z)
    );

    const Matrix rotMat = XMMatrixRotationRollPitchYaw(rotRad.x, rotRad.y, rotRad.z);
    const Vec3 worldDelta = Vec3::TransformNormal(delta, rotMat);

    transform->Set_Position(currentPos + worldDelta);
}

void BattleActionExecutor::Update_RootMotionToTarget(BattleAction& action, const Shared<TransformCom>& transform, const Vec3& currentPos)
{
    //RootMotionToTarget
    Vec3 toGoal = action.GetPhaseGoalPos() - currentPos;
    toGoal.y = 0.f;

    const float remain = toGoal.Length();
    if (remain <= 0.05f)
    {
        FinishCurrentPhaseAtGoal(action);
        return;
    }

    const auto& rm = action.Get_CurChar().ModelCom->Get_RootTransformVelocity();
    Vec3 localDelta = rm.position * 0.01f;
    localDelta.y = 0.f;

    if (localDelta.LengthSquared() <= 0.0001f)
        return;

    const Vec3 rotDeg = transform->Get_RotationEuler();
    const Vec3 rotRad(
        XMConvertToRadians(rotDeg.x),
        XMConvertToRadians(rotDeg.y),
        XMConvertToRadians(rotDeg.z)
    );

    const Matrix rotMat = XMMatrixRotationRollPitchYaw(rotRad.x, rotRad.y, rotRad.z);
    Vec3 worldDelta = Vec3::TransformNormal(localDelta, rotMat);
    worldDelta.y = 0.f;

    if (worldDelta.LengthSquared() <= 0.0001f)
        return;

    Vec3 goalDir = toGoal;
    goalDir.Normalize();

    const float forwardAmount = worldDelta.Dot(goalDir);
    if (forwardAmount <= 0.f)
        return;

    const Vec3 moveDelta = goalDir * forwardAmount;
    if (moveDelta.Length() >= remain)
    {
        FinishCurrentPhaseAtGoal(action);
        return;
    }

    transform->Set_Position(currentPos + moveDelta);
}

void BattleActionExecutor::Update_RatioMoveTo(BattleAction& action, const Shared<TransformCom>& transform, const Vec3& currentPos)
{
    //ReturnToOrigin
    //MoveToTarget
    Vec3 path = action.GetPhaseGoalPos() - action.GetPhaseStartPos();
    path.y = 0.f;
    const float totalDistance = path.Length();

    if (totalDistance <= 0.05f)
    {
       // FinishCurrentPhaseAtGoal(action);
      /*  action.SnapActorToPhaseGoal();
        action.ResetPhasePrevRootRatio();
        action.SetPhaseReachedGoal(true);*/
        return;
    }

    path.Normalize();

    Vec3 toGoal = action.GetPhaseGoalPos() - currentPos;
    toGoal.y = 0.f;
    const float remain = toGoal.Length();

    if (remain <= 0.05f)
    {
      //  FinishCurrentPhaseAtGoal(action);
      /*  action.SnapActorToPhaseGoal();
        action.ResetPhasePrevRootRatio();
        action.SetPhaseReachedGoal(true);*/
        return;
    }

    const float currentRatio = action.Get_CurChar().ModelCom->Get_RootMotionTranslationRatio();
    float deltaRatio = currentRatio - action.GetPhasePrevRootRatio();

    //if (currentRatio >= 0.999f)
    //{
    //  //  FinishCurrentPhaseAtGoal(action);
    //   /* action.SnapActorToPhaseGoal();
    //    action.ResetPhasePrevRootRatio();
    //    action.SetPhaseReachedGoal(true);*/
    //    return;
    //}

    if (deltaRatio < 0.f)
        deltaRatio = 0.f;

    action.SetPhasePrevRootRatio(currentRatio);

    //if (deltaRatio <= 0.f /*|| currentRatio >= 0.999f*/)
    //    return;

    const float moveAmount = totalDistance * deltaRatio;
    if (moveAmount <= 0.f)
        return;

    //if (moveAmount >= remain)
    //{
    //  //  FinishCurrentPhaseAtGoal(action);
    //  /*  action.SnapActorToPhaseGoal();
    //    action.ResetPhasePrevRootRatio();
    //    action.SetPhaseReachedGoal(true);*/
    //    return;
    //}

    const float clampedMove = min(moveAmount, remain);
    transform->Set_Position(currentPos + path * clampedMove);
}

void BattleActionExecutor::FinishCurrentPhaseAtGoal(BattleAction& action)
{
    if (!action.Get_CurChar().actor)
        return;

    action.SnapActorToPhaseGoal();
    action.ResetPhasePrevRootRatio();
    action.SetPhaseReachedGoal(true);
    Complete_CurPhase(action);
}

Shared<BattleActionExecutor> BattleActionExecutor::Create()
{
    return make_shared<BattleActionExecutor>();
}

void BattleActionExecutor::Free()
{
    Base::Free();
}
