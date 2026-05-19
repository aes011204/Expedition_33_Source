#include "BattleActionExecutor.h"
#include "Character.h"

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

    Advance_ToNextPhase(action);
}
void BattleActionExecutor::Advance_ToNextPhase(Action& action)
{
    const auto& phases = action._selected_SkillKey.AnimPhases;
    if (action._curAnimIndex >= phases.size())
        return;

    //TODO :: 나중에 워크 고치고 주석 풀기
    //if (action._curChar.actor)
    //    action._curChar.actor->Get_Transform()->Set_Position(action.phaseGoalPos);

    action._curAnimIndex++;

    if (action._curAnimIndex >= phases.size())
    {
        if (action._curChar.ModelCom && !action._selected_SkillKey.IdleAnimationKey.empty())
            action._curChar.ModelCom->Set_Animation(action._selected_SkillKey.IdleAnimationKey, true, 0.f);

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
        action._curChar.ModelCom->Set_Animation(phase.animationKey, true, 0.f);
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
    //if (phase.movementPolicy == BattleMovementPolicy::None)
    //    return;

    //LOG_INFO("{}", action._curAnimIndex);

    if (!action._curChar.actor || !action._curChar.ModelCom)
        return;

    const auto transform = action._curChar.actor->Get_Transform();
    const Vec3 currentPos = transform->Get_Position();

    if (phase.faceTarget)
        Face_ActionTarget(action, action.phaseGoalPos);

    if (phase.movementPolicy == BattleMovementPolicy::None)
    {
        ///TODO :: 지금 100 곱한거는 임시 정확한 규칙 찾아야함 + 현제 회전에 맞게 변형해서 곱해야함
        const auto& rm = action._curChar.ModelCom->Get_RootTransformVelocity();
        const Vec3 delta = rm.position * 100;

        LOG_INFO("position : {}", rm.position.z);

        Vec3 rotDeg = transform->Get_RotationEuler();
        Vec3 rotRad = Vec3(
            XMConvertToRadians(rotDeg.x),
            XMConvertToRadians(rotDeg.y),
            XMConvertToRadians(rotDeg.z)
        );
        Matrix rotMat = XMMatrixRotationRollPitchYaw(rotRad.x, rotRad.y, rotRad.z);
        Vec3 worldDelta = Vec3::TransformNormal(delta, rotMat);

        transform->Set_Position(currentPos + worldDelta);

        return;
    }


    Vec3 path = action.phaseGoalPos - action.phaseStartPos;
    path.y = 0.f;
    const float totalDistance = path.Length();

    if (totalDistance <= 0.05f)
    {
        transform->Set_Position(action.phaseGoalPos);
        action.phaseReachedGoal = true;
        Advance_ToNextPhase(action);
        return;
    }

    path.Normalize();

    Vec3 toGoal = action.phaseGoalPos - currentPos;
    toGoal.y = 0.f;

    const float remain = toGoal.Length();
    if (remain <= 0.05f)
    {
        transform->Set_Position(action.phaseGoalPos);
        action.phaseReachedGoal = true;
        Advance_ToNextPhase(action);
        return;
    }


    if (phase.movementPolicy == BattleMovementPolicy::RootMotionToTarget)
    {
        const auto& rm = action._curChar.ModelCom->Get_RootTransformVelocity();
        Vec3 localDelta = rm.position * 100.f;
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
            transform->Set_Position(action.phaseGoalPos);
            action.phaseReachedGoal = true;
            Advance_ToNextPhase(action);
        }
        else
        {
            transform->Set_Position(currentPos + moveDelta);
        }

        return;
    }
    if (phase.movementPolicy == BattleMovementPolicy::MoveToTarget || phase.movementPolicy == BattleMovementPolicy::ReturnToOrigin)
    {
        const float currentRatio = action._curChar.ModelCom->Get_RootMotionTranslationRatio();
        float deltaRatio = currentRatio - action.phasePrevRootRatio;
        if (currentRatio >= 0.999f)
        {
            transform->Set_Position(action.phaseGoalPos);
            action.phaseReachedGoal = true;
            Advance_ToNextPhase(action);
            return;
        }
        if (deltaRatio < 0.f)
            deltaRatio = 0.f;

        action.phasePrevRootRatio = currentRatio;
        // 애니메이션이 바뀌었거나 루프되어 비율이 역전된 경우 또는 끝난 경우 처리
        if (deltaRatio < -0.05f || currentRatio >= 0.999f)
            return;

        const float moveAmount = totalDistance * deltaRatio;
        if (moveAmount <= 0.f)
            return;

        if (moveAmount >= remain)
        {
            transform->Set_Position(action.phaseGoalPos);
            action.phaseReachedGoal = true;
            Advance_ToNextPhase(action);
        }
        else
            transform->Set_Position(currentPos + path * moveAmount);
    }


}

Shared<BattleActionExecutor> BattleActionExecutor::Create()
{
    return make_shared<BattleActionExecutor>();
}

void BattleActionExecutor::Free()
{
    Base::Free();
}
