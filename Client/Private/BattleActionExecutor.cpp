#include "BattleActionExecutor.h"

#include "BattleAnimutils.h"
#include "Character.h"
#include "CombatStatCom.h"

void BattleActionExecutor::BeginAction(Action& action)
{
    _isFinished = false;
    action._curAnimIndex = 0;
    Begin_ActionPhase(action);
}

void BattleActionExecutor::Update(float timeDelta, Action& action)
{
    if (_isFinished)
        return;


    Update_ActionMovement(timeDelta, action);
}

void BattleActionExecutor::OnActionEndNotify(Action& action)
{
    const auto& phases = action._selected_SkillKey.AnimPhases;
    if (action._curAnimIndex >= phases.size())
        return;

    const BattleActionPhase& phase = phases[action._curAnimIndex];

    if (phase.movementPolicy == BattleMovementPolicy::RootMotionToTarget ||
        phase.movementPolicy == BattleMovementPolicy::MoveToTarget ||
        phase.movementPolicy == BattleMovementPolicy::ReturnToOrigin)
    {
        return;
    }

    Complete_CurPhase(action);
}

void BattleActionExecutor::Complete_CurPhase(Action& action)
{
    const auto& phases = action._selected_SkillKey.AnimPhases;
    if (action._curAnimIndex >= phases.size())
        return;

    action._curAnimIndex++;

    if (action._curAnimIndex >= phases.size())
    {
        if (action._curChar.ModelCom)
        {
            BattleAnimUtils::Play_BattleIdle(action._curChar, 0.3f);
            //action._curChar.ModelCom->Set_Animation(action._selected_SkillKey.IdleAnimationKey, true, 0.f);
        }

        _isFinished = true;
        action.phaseReachedGoal = true;
        return;
    }

    Begin_ActionPhase(action);
}

void BattleActionExecutor::Begin_ActionPhase(Action& action)
{
    if (!action._curChar.actor)
        return;

    //phases 가져옴
    const auto phases = action._selected_SkillKey.AnimPhases;
    if (phases.size() == 0)
        return;

    const BattleActionPhase& phase = phases[action._curAnimIndex];

    //phase에 현재 단계 데이터 저장
    action.phaseStartPos = action._curChar.actor->Get_Transform()->Get_Position();
    action.phaseGoalPos = Resolve_PhaseGoalPosition(action, phase);
    action.phaseReachedGoal = false;
    action.phasePrevRootRatio = 0.f;

    // _testPrevRatio = action._curChar.ModelCom->Get_RootMotionTranslationRatio();

    if (phase.faceTarget)
        Face_ActionTarget(action, action.phaseGoalPos);


    if(phase.movementPolicy ==BattleMovementPolicy::RootMotionToTarget)
    {
        action._curChar.ModelCom->Set_Animation(phase.animationKey,  true, 0.f);
    }
    else
    action._curChar.ModelCom->Set_Animation(phase.animationKey, false, 0.f);
}

Vec3 BattleActionExecutor::Resolve_AnchorPosition(const Action& action, BattleTargetAnchor anchor) const
{
    switch (anchor)
    {
    case BattleTargetAnchor::Self:
        return action._curChar.actor->Get_Transform()->Get_Position();

    case BattleTargetAnchor::PrimaryTarget:
        if (action.target.actor)
            return action.target.actor->Get_Transform()->Get_Position();
        break;

    case BattleTargetAnchor::TargetCenter: // TODO:: 나중에 팀의 중심 값가져오기
        if (action.target.actor)
            return action.target.actor->Get_Transform()->Get_Position();
        break;

    default:
        break;
    }

    return action._curChar.actor->Get_Transform()->Get_Position();
}

Vec3 BattleActionExecutor::Resolve_PhaseGoalPosition(const Action& action, const BattleActionPhase& phase) const
{
    if (!action._curChar.actor)
        return {};

    const Vec3 actorPos = action._curChar.actor->Get_Transform()->Get_Position();

    if (phase.movementPolicy == BattleMovementPolicy::ReturnToOrigin)
        return action.originPos;

    const Vec3 anchorPos = Resolve_AnchorPosition(action, phase.targetAnchor);

    if (!phase.useDistance)
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

void BattleActionExecutor::Face_ActionTarget(Action& action, const Vec3& goalPos)
{
    if (!action._curChar.actor)
        return;

    const auto transform = action._curChar.actor->Get_Transform();
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

void BattleActionExecutor::Update_ActionMovement(float timeDelta, Action& action)
{
    const auto& phases = action._selected_SkillKey.AnimPhases;
    if (action._curAnimIndex >= phases.size())
        return;

    const BattleActionPhase& phase = phases[action._curAnimIndex];
    if (phase.movementPolicy == BattleMovementPolicy::Custom)
        return;

    if (!action._curChar.actor || !action._curChar.ModelCom)
        return;

    const auto transform = action._curChar.actor->Get_Transform();
    const Vec3 currentPos = transform->Get_Position();

    if (phase.faceTarget)
        Face_ActionTarget(action, action.phaseGoalPos);

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

void BattleActionExecutor::Update_BasicMovement(Action& action, const Shared<TransformCom>& transform, const Vec3& currentPos)
{
    //NONE
    const auto& rm = action._curChar.ModelCom->Get_RootTransformVelocity();
    const Vec3 delta = rm.position * 0.01f;

    Vec3 rotDeg = transform->Get_RotationEuler();
    Vec3 rotRad(
        XMConvertToRadians(rotDeg.x),
        XMConvertToRadians(rotDeg.y),
        XMConvertToRadians(rotDeg.z));

    Matrix rotMat = XMMatrixRotationRollPitchYaw(rotRad.x, rotRad.y, rotRad.z);
    Vec3 worldDelta = Vec3::TransformNormal(delta, rotMat);

    transform->Set_Position(currentPos + worldDelta);
}

void BattleActionExecutor::Update_RootMotionToTarget(Action& action, const Shared<TransformCom>& transform, const Vec3& currentPos)
{

    //RootMotionToTarget
    Vec3 toGoal = action.phaseGoalPos - currentPos;
    toGoal.y = 0.f;

    const float remain = toGoal.Length();
    if (remain <= 0.05f)
    {
        FinishCurrentPhaseAtGoal(action);
        return;
    }

    const auto& rm = action._curChar.ModelCom->Get_RootTransformVelocity();
    Vec3 localDelta = rm.position * 0.01f;
    localDelta.y = 0.f;

    if (localDelta.LengthSquared() <= 0.0001f)
        return;

    Vec3 rotDeg = transform->Get_RotationEuler();
    Vec3 rotRad(
        XMConvertToRadians(rotDeg.x),
        XMConvertToRadians(rotDeg.y),
        XMConvertToRadians(rotDeg.z));

    Matrix rotMat = XMMatrixRotationRollPitchYaw(rotRad.x, rotRad.y, rotRad.z);
    Vec3 worldDelta = Vec3::TransformNormal(localDelta, rotMat);
    worldDelta.y = 0.f;

    if (worldDelta.LengthSquared() <= 0.0001f)
        return;

    Vec3 goalDir = toGoal;
    goalDir.Normalize();

    const float forwardAmount = worldDelta.Dot(goalDir);
    if (forwardAmount <= 0.f)
        return;

    Vec3 moveDelta = goalDir * forwardAmount;
    if (moveDelta.Length() >= remain)
    {
        FinishCurrentPhaseAtGoal(action);
        return;
    }

    transform->Set_Position(currentPos + moveDelta);
}

void BattleActionExecutor::Update_RatioMoveTo(Action& action, const Shared<TransformCom>& transform, const Vec3& currentPos)
{
    //ReturnToOrigin
    //MoveToTarget
    Vec3 path = action.phaseGoalPos - action.phaseStartPos;
    path.y = 0.f;
    const float totalDistance = path.Length();

    if (totalDistance <= 0.05f)
    {
        FinishCurrentPhaseAtGoal(action);
        return;
    }

    path.Normalize();

    Vec3 toGoal = action.phaseGoalPos - currentPos;
    toGoal.y = 0.f;
    const float remain = toGoal.Length();

    if (remain <= 0.05f)
    {
        FinishCurrentPhaseAtGoal(action);
        return;
    }

    const float currentRatio = action._curChar.ModelCom->Get_RootMotionTranslationRatio();
    float deltaRatio = currentRatio - action.phasePrevRootRatio;

    if (currentRatio >= 0.999f)
    {
        FinishCurrentPhaseAtGoal(action);
        return;
    }

    if (deltaRatio < 0.f)
        deltaRatio = 0.f;

    action.phasePrevRootRatio = currentRatio;

    if (deltaRatio < -0.05f || currentRatio >= 0.999f)
        return;

    const float moveAmount = totalDistance * deltaRatio;
    if (moveAmount <= 0.f)
        return;

    if (moveAmount >= remain)
    {
        FinishCurrentPhaseAtGoal(action);
        return;
    }

    transform->Set_Position(currentPos + path * moveAmount);
}

void BattleActionExecutor::FinishCurrentPhaseAtGoal(Action& action)
{
    if (!action._curChar.actor)
        return;

    action._curChar.actor->Get_Transform()->Set_Position(action.phaseGoalPos);
    action.phaseReachedGoal = true;
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
