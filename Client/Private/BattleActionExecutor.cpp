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

    if (action._curChar.actor)
        action._curChar.actor->Get_Transform()->Set_Position(action.phaseGoalPos);

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

    if (phase.faceTarget)
        Face_ActionTarget(action, action.phaseGoalPos);

    action._curChar.ModelCom->Set_Animation(phase.animationKey, false, 1.5f);

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

    Vec3 anchorPos = Resolve_AnchorPosition(action, phase.targetAnchor);


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

    auto transform = action._curChar.actor->Get_Transform();
    Vec3 pos = transform->Get_Position();

    Vec3 dir = goalPos - pos;
    dir.y = 0.f;

    if (dir.LengthSquared() <= 0.0001f)
        return;

    dir.Normalize();

    const float yaw = atan2f(dir.x, dir.z);
    Vec3 rot = transform->Get_RotationEuler();
    rot.y = yaw;
    transform->Set_RotationEuler(rot);
}

void BattleActionExecutor::Update_ActionMovement(float timeDelta, Action& action)
{

    const auto& phases = action._selected_SkillKey.AnimPhases;
    if (action._curAnimIndex >= phases.size())
        return;

    const BattleActionPhase& phase = phases[action._curAnimIndex];
    if (phase.movementPolicy == BattleMovementPolicy::None)
        return;

    if (!action._curChar.actor || !action._curChar.ModelCom)
        return;

    auto transform = action._curChar.actor->Get_Transform();
    Vec3 currentPos = transform->Get_Position();

    Vec3 toGoal = action.phaseGoalPos - currentPos;
    toGoal.y = 0.f;

    const float remain = toGoal.Length();
    if (remain <= 0.05f)
    {
        transform->Set_Position(action.phaseGoalPos);
        action.phaseReachedGoal = true;
        return;
    }

    toGoal.Normalize();

    float step = 0.f;
    const auto rm = action._curChar.ModelCom->Get_RootTransformVelocity();
    step = rm.position.Length();

    if (step <= 0.f)
        return;

    Vec3 delta = toGoal * step;

    if (delta.Length() >= remain)
    {
        transform->Set_Position(action.phaseGoalPos);
        action.phaseReachedGoal = true;
    }
    else
    {
        transform->Set_Position(currentPos + delta);
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
