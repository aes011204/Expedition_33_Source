#include "BattleActionExecutor.h"

#include "BattleAnimutils.h"
#include "Battle_Event.h"
#include "Character.h"
#include "Monster_BattleBase.h"
#include "CombatStatCom.h"
#include "Event_Bus.h"

void BattleActionExecutor::BeginAction(BattleAction& action)
{
    Reset_ActivePhasePlayRate();
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
    if (action.GetCurAnimIndex() >= phases.size())
        return;

    const BattleActionPhase& phase = phases[action.GetCurAnimIndex()];

    if (phase.movementPolicy == BattleMovementPolicy::RootMotionToTarget)
    {
        // EndAnim notify fallback은 위치를 스냅하지 않고 phase만 넘긴다.
        // RootMotion 도중 목표점으로 뚝 보정되는 움직임을 막기 위함이다.
        Complete_CurPhase(action);
        return;
    }

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
        Reset_ActivePhasePlayRate();

        if (action.Get_CurChar().ModelCom)
        {
            BattleAnimUtils::Play_BattleIdle(action.Get_CurChar(), 0.3f);
        }

        _isFinished = true;
        _hasTriggeredCloseApproachShot = false;
        action.SetPhaseReachedGoal(true);
        return;
    }

    Begin_ActionPhase(action);
}

void BattleActionExecutor::Begin_ActionPhase(BattleAction& action)
{
    if (!action.Get_CurChar().actor || !action.Get_CurChar().ModelCom)
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

    if (phase.faceTarget &&
        phase.movementPolicy != BattleMovementPolicy::MoveToTarget &&
        phase.movementPolicy != BattleMovementPolicy::ReturnToOrigin &&
        phase.movementPolicy != BattleMovementPolicy::RootMotionToTarget)
    {
        const Vec3 faceGoalPos =
            phase.targetAnchor == BattleTargetAnchor::Self
                ? Resolve_AnchorPosition(action, BattleTargetAnchor::TargetCenter)
                : action.GetPhaseGoalPos();

        Face_ActionTarget(action, faceGoalPos);
    }

    Apply_PhasePlayRate(phase, action.Get_CurChar().ModelCom);

    if (phase.movementPolicy == BattleMovementPolicy::RootMotionToTarget)
        action.Get_CurChar().ModelCom->Set_Animation(phase.animationKey, true);
    else
        action.Get_CurChar().ModelCom->Set_Animation(phase.animationKey, false);
}

Vec3 BattleActionExecutor::Resolve_AnchorPosition(const BattleAction& action, BattleTargetAnchor anchor) const
{
    Vec3 fallbackPos = Vec3::Zero;
    if (action.Get_CurChar().actor != nullptr)
    {
        const Shared<TransformCom> transform = action.Get_CurChar().actor->Get_Transform();
        if (transform != nullptr)
            fallbackPos = transform->Get_Position();
    }

    switch (anchor)
    {
    case BattleTargetAnchor::Self:
        return fallbackPos;

    case BattleTargetAnchor::PrimaryTarget:
        if (action.GetInitTarget().actor)
        {
            const Shared<TransformCom> transform = action.GetInitTarget().actor->Get_Transform();
            if (transform != nullptr)
                return transform->Get_Position();
        }
        break;

    case BattleTargetAnchor::TargetCenter:
    {
        Vec3 center = Vec3::Zero;
        uint32 targetCount = 0;

        const auto accumulateTargetPosition = [&](const BattleChar& target)
        {
            if (target.actor == nullptr)
                return;

            const Shared<TransformCom> transform = target.actor->Get_Transform();
            if (transform == nullptr)
                return;

            center += transform->Get_Position();
            ++targetCount;
        };

        const auto& steps = action.GetSteps();
        const uint32 curStepIndex = action.GetCurStepIndex();
        if (curStepIndex < steps.size())
        {
            for (const BattleChar& target : steps[curStepIndex].targets)
                accumulateTargetPosition(target);
        }

        if (targetCount > 0)
            return center / static_cast<float>(targetCount);

        if (action.GetInitTarget().actor)
        {
            const Shared<TransformCom> transform = action.GetInitTarget().actor->Get_Transform();
            if (transform != nullptr)
                return transform->Get_Position();
        }
        break;
    }
    case BattleTargetAnchor::Origin:        // TODO:: 나중에 팀의 중심 값가져오기
            return action.GetOriginPos();
        break;
    default:
        break;
    }

    return fallbackPos;
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

void BattleActionExecutor::SmoothFaceActionTarget(BattleAction& action, const Vec3& goalPos, float timeDelta)
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
    const float goalYawDeg = XMConvertToDegrees(yawRad);

    Vec3 rot = transform->Get_RotationEuler();
    float deltaYaw = goalYawDeg - rot.y;
    while (deltaYaw > 180.f)
        deltaYaw -= 360.f;
    while (deltaYaw < -180.f)
        deltaYaw += 360.f;

    const float turnSpeed = 360.f * timeDelta;
    if (fabsf(deltaYaw) <= turnSpeed)
        rot.y = goalYawDeg;
    else
        rot.y += (deltaYaw > 0.f ? turnSpeed : -turnSpeed);

    transform->Set_RotationEuler(rot);
}

void BattleActionExecutor::Apply_PhasePlayRate(const BattleActionPhase& phase, const Shared<Engine::ModelCom>& model)
{
    if (model == nullptr)
        return;

    if (_activePhaseModel != nullptr && _activePhaseModel != model)
        _activePhaseModel->Set_FrameSpeed(1.f);

    _activePhaseModel = model;

    const float playRate = phase.playRate > 0.f ? phase.playRate : 1.f;
    model->Set_FrameSpeed(playRate);
}

void BattleActionExecutor::Reset_ActivePhasePlayRate()
{
    if (_activePhaseModel)
        _activePhaseModel->Set_FrameSpeed(1.f);

    _activePhaseModel.reset();
}

void BattleActionExecutor::Update_ActionMovement(float timeDelta, BattleAction& action)
{
    if (action.HasReachedPhaseGoal())
        return;

    const auto& phases = action.GetSkillData().AnimPhases;
    if (action.GetCurAnimIndex() >= phases.size())
        return;

    const BattleActionPhase& phase = phases[action.GetCurAnimIndex()];

    if (!action.Get_CurChar().actor || !action.Get_CurChar().ModelCom)
        return;

    const auto transform = action.Get_CurChar().actor->Get_Transform();
    const Vec3 currentPos = transform->Get_Position();

    if (phase.faceTarget)
    {
        const Vec3 faceGoalPos =
            phase.targetAnchor == BattleTargetAnchor::Self
                ? Resolve_AnchorPosition(action, BattleTargetAnchor::TargetCenter)
                : action.GetPhaseGoalPos();

        SmoothFaceActionTarget(action, faceGoalPos, timeDelta);
    }

    if (phase.movementPolicy == BattleMovementPolicy::Custom)
        return;

    switch (phase.movementPolicy)
    {
    case BattleMovementPolicy::None:
        Update_BasicMovement(action, transform, currentPos);
        return;

    case BattleMovementPolicy::RootMotionToTarget:
        Update_RootMotionToTarget(action, transform, currentPos, timeDelta);
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
    //const auto& rm = action.Get_CurChar().ModelCom->Get_RootTransformVelocity();
    //const Vec3 delta = rm.position * 0.01f;

    //const Vec3 rotDeg = transform->Get_RotationEuler();
    //const Vec3 rotRad(
    //    XMConvertToRadians(rotDeg.x),
    //    XMConvertToRadians(rotDeg.y),
    //    XMConvertToRadians(rotDeg.z)
    //);

    //const Matrix rotMat = XMMatrixRotationRollPitchYaw(rotRad.x, rotRad.y, rotRad.z);
    //const Vec3 worldDelta = Vec3::TransformNormal(delta, rotMat);

    //transform->Set_Position(currentPos + worldDelta);
}

void BattleActionExecutor::Update_RootMotionToTarget(BattleAction& action, const Shared<TransformCom>& transform, const Vec3& currentPos, float timeDelta)
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

    const Skill_Data& skillData = action.GetSkillData();
    const uint32 curPhaseIndex = action.GetCurAnimIndex();

    if(!_hasTriggeredCloseApproachShot &&
        skillData.skillKey == "Abbest_Knife"&&
        curPhaseIndex == 0 &&
        remain <= 4.5f)
    {
        EvtBattlePresentation event{};
        event.cameraMode = BattleCameraMode::ENEMYATTACK;
        event.subject = action.Get_CurChar().actor;
        event.opponent = action.GetInitTarget().actor;
        event.useClip = true;
        event.restartClip = true;
        event.presetId = "Abbest_DashIn";
        event.clipId = "Abbest_DashIn";

        GAME->Get_EventBus().Publish(event);
        _hasTriggeredCloseApproachShot = true;
    }


    Vec3 goalDir = toGoal;
    goalDir.Normalize();

    const auto& rm = action.Get_CurChar().ModelCom->Get_RootTransformVelocity();
    Vec3 localDelta = rm.position * 0.01f;
    localDelta.y = 0.f;

    Vec3 moveDelta = Vec3::Zero;

    if (localDelta.LengthSquared() > 0.0001f)
    {
        const Vec3 rotDeg = transform->Get_RotationEuler();
        const Vec3 rotRad(
            XMConvertToRadians(rotDeg.x),
            XMConvertToRadians(rotDeg.y),
            XMConvertToRadians(rotDeg.z)
        );

        const Matrix rotMat = XMMatrixRotationRollPitchYaw(rotRad.x, rotRad.y, rotRad.z);
        Vec3 worldDelta = Vec3::TransformNormal(localDelta, rotMat);
        worldDelta.y = 0.f;

        if (worldDelta.LengthSquared() > 0.0001f)
        {
            const float forwardAmount = worldDelta.Dot(goalDir);
            if (forwardAmount > 0.f)
                moveDelta = goalDir * forwardAmount;
        }
    }
    else
    {
        const float fallbackSpeed = transform->Get_SpeedPerSec();
        if (fallbackSpeed > 0.f)
            moveDelta = goalDir * fallbackSpeed * timeDelta;
    }

    if (moveDelta.LengthSquared() <= 0.0001f)
        return;

    //const float forwardAmount = worldDelta.Dot(goalDir);
    //if (forwardAmount <= 0.f)
    //    return;

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
        return;
    }

    path.Normalize();

    Vec3 toGoal = action.GetPhaseGoalPos() - currentPos;
    toGoal.y = 0.f;
    const float remain = toGoal.Length();

    if (remain <= 0.05f)
    {
        return;
    }

    const float currentRatio = action.Get_CurChar().ModelCom->Get_RootMotionTranslationRatio();
    float deltaRatio = currentRatio - action.GetPhasePrevRootRatio();



    //if (deltaRatio < 0.f)
    //    deltaRatio = 0.f;

    action.SetPhasePrevRootRatio(currentRatio);


    const float moveAmount = totalDistance * deltaRatio;
 /*   if (moveAmount <= 0.f)
        return;*/

 
    const float clampedMove = min(moveAmount, remain);



    const auto& rm = action.Get_CurChar().ModelCom->Get_RootTransformVelocity();
    Vec3 right = transform->Get_WorldRight();
    right.y = 0.f;
    right.Normalize();
    transform->Set_Position(currentPos + path * clampedMove + right * (rm.position.x * 0.01f));
   // transform->Set_Position(currentPos + path * clampedMove);

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
