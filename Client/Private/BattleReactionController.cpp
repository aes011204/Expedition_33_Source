#include "BattleReactionController.h"
#include <Battle_Event.h>
#include "CombatStatCom.h"
#include "DamageSystem.h"
#include "ClientInstance.h"
#include "BattleController.h"
#include "BattleSpawnRegistry.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "Event_Bus.h"
#include "TransformCom.h"

void BattleReactionController::OpenWindow(BattleReactionType type)
{
    if (type == BattleReactionType::Dodge)
        _isDodgeOpen = true;
    else if (type == BattleReactionType::Parry)
        _isParryOpen = true;
    else
        return;

    OnReactionWindowOpened.Broadcast(type);
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
    ResetCounterTimeSection();

    for (const BattleChar& target : _reactionAnimTargets)
    {
        if (target.ModelCom)
            target.ModelCom->Set_FrameSpeed(1.f);
    }
    _reactionAnimTargets.clear();
    _counterReturnRuntimes.clear();
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

bool BattleReactionController::TryConsumeGuard(const BattleChar& target, uint32 stepIndex)
{
    if (target.actor == nullptr || target.statCom == nullptr)
        return false;

    if (!target.statCom->Is_Player())
        return false;

    BattleReactionType result = BattleReactionType::None;

    return TryConsume(result, stepIndex);
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

BattleReactionController::BattleReactionInputResult BattleReactionController::SubmitInput(BattleReactionType type,
    const BattleAction& turnAction)
{
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

    const BattleChar& counterTarget = turnAction.Get_CurChar();
    const uint32 curStepIndex = turnAction.GetCurStepIndex();

    if (type == BattleReactionType::Dodge)
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
    else if (type == BattleReactionType::Parry)
    {
        const bool success = TryInput(BattleReactionType::Parry, curStepIndex);

        result.hasInput = true;
        result.type = BattleReactionType::Parry;
        result.success = success;

        if (success)
        {
            _reactionAnimTargets.clear();
            _counterReturnRuntimes.clear();
            _counterDamageTarget = {};
            _isCounterPending = false;
            _isCounterDamageApplied = false;
            _animPhase = ReactionAnimPhase::None;
        }

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

            if (success && target.actor != nullptr)
            {
                // 패링 성공 시 짧게
                if (auto battle = CLIENT->Get_BattleController())
                    battle->Play_BattleHitStop(0.08f, 0.08f);

                PlayEffectDesc effectDesc{};
                effectDesc.effectName = L"Effects\\Assets\\Distortion\\Double_Distotion.effect.json";
                effectDesc.spawnBasisTarget = target.actor;
                effectDesc.scale = Vec3(0.5f, 0.5f, 0.5f);
                effectDesc.localOffset = Vec3(0.f, 0.7f, 0.f);

                CLIENT->Play_Effect(effectDesc);
            }

            if (success &&
                result.apRewardAmount == 0 &&
                target.actor != nullptr &&
                target.statCom != nullptr &&
                target.statCom->Is_Player())
            {
                result.apRewardTarget = target;
                result.apRewardAmount = 1;
                result.apRewardReason = EAPChangeReason::Parry;
            }
        }
    }

    if (result.success)
        OnReactionSucceeded.Broadcast(result.type, turnAction.GetCurStepTarget());

    return result;
}

BattleReactionController::BattleReactionInputResult BattleReactionController::UpdateInput(const BattleAction& turnAction)
{
    if (GAME->KeyDown(KEY_TYPE::Q))
        return SubmitInput(BattleReactionType::Dodge, turnAction);

    if (GAME->KeyDown(KEY_TYPE::E))
        return SubmitInput(BattleReactionType::Parry, turnAction);

    return {};
}

void BattleReactionController::UpdateAnimation(float timeDelta)
{
    if (_reactionAnimTargets.empty())
        return;

    if (_animPhase == ReactionAnimPhase::CounterReturn)
    {
        UpdateCounterReturn(timeDelta);
        return;
    }

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
    {

        return;
    }

    switch (_animPhase)
    {
    case ReactionAnimPhase::Reaction:
        FinishReactionAnimation();
        break;

    case ReactionAnimPhase::Counter:
        StartCounterReturn();
        break;

    case ReactionAnimPhase::CounterReturn:
        break;

    default:
        FinishReactionAnimation();
        break;
    }
}

void BattleReactionController::StartCounterReturn()
{
    if (_animPhase != ReactionAnimPhase::Counter)
        return;

    if (_reactionAnimTargets.size() <= 1)
    {
        FinishReactionAnimation();
        return;
    }

    _counterReturnRuntimes.clear();

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
    {
        FinishReactionAnimation();
        return;
    }

    const Shared<BattleSpawnRegistry> spawnRegistry = battle->Get_BattleSpawnRegistry();
    if (spawnRegistry == nullptr)
    {
        FinishReactionAnimation();
        return;
    }

    bool hasReturnTarget = false;

    for (const BattleChar& target : _reactionAnimTargets)
    {
        if (target.actor == nullptr || target.ModelCom == nullptr || target.statCom == nullptr)
            continue;

        if (target.statCom->Get_AliveStatus() == EAliveStatus::Dead)
            continue;

        const BattleSpawnRegistry::BattleEntry* spawnEntry = spawnRegistry->Find_SpawnEntry(target.actor);
        if (spawnEntry == nullptr)
        {
            TryRestoreTargetToSpawnPose(target);
            continue;
        }

        const Shared<TransformCom> transform = target.actor->Get_Transform();
        if (transform == nullptr)
        {
            TryRestoreTargetToSpawnPose(target);
            continue;
        }

        CounterReturnRuntime runtime{};
        runtime.target = target;
        runtime.startPos = transform->Get_Position();
        runtime.goalPos = spawnEntry->spawnPosition;
        runtime.goalRot = spawnEntry->spawnRotation;
        runtime.prevRootRatio = 0.f;
        _counterReturnRuntimes.push_back(runtime);
        hasReturnTarget = true;

        target.ModelCom->Set_RootMotionEnabled(true);
        target.ModelCom->Set_FrameSpeed(1.f);

        LOG_INFO(
            "[BattleCamTrace] StartCounterReturn prefab={} anim=Return targetAlive={} phaseBefore={}",
            target.actor ? target.actor->Get_SourcePrefabName().c_str() : "",
            target.statCom && target.statCom->Get_AliveStatus() == EAliveStatus::Alive,
            static_cast<int>(_animPhase));

        if (!target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Return), 0.15f, false))
        {
            TryRestoreTargetToSpawnPose(target);
            FinishReactionAnimation();
            return;
        }
    }

    if (!hasReturnTarget)
    {
        FinishReactionAnimation();
        return;
    }

    _animPhase = ReactionAnimPhase::CounterReturn;
}
void BattleReactionController::UpdateCounterReturn(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);



    ///////////////////////
    ///
        //ReturnToOrigin
    //MoveToTarget

    if (_animPhase != ReactionAnimPhase::CounterReturn)
        return;

    bool allReached = true;

    for (const BattleChar& target : _reactionAnimTargets)
    {
        if (target.actor == nullptr || target.ModelCom == nullptr || target.statCom == nullptr)
            continue;

        if (target.statCom->Get_AliveStatus() == EAliveStatus::Dead)
            continue;

        CounterReturnRuntime* runtime = nullptr;
        for (CounterReturnRuntime& candidate : _counterReturnRuntimes)
        {
            if (candidate.target.actor == target.actor)
            {
                runtime = &candidate;
                break;
            }
        }

        if (runtime == nullptr)
        {
            TryRestoreTargetToSpawnPose(target);
            continue;
        }

        const Shared<TransformCom> transform = target.actor->Get_Transform();
        if (transform == nullptr)
        {
            TryRestoreTargetToSpawnPose(target);
            continue;
        }

        Vec3 currentPos = transform->Get_Position();

        // ReturnToOrigin / MoveToTarget 방식
        Vec3 path = runtime->goalPos - runtime->startPos;
        path.y = 0.f;
        const float totalDistance = path.Length();

        if (totalDistance <= 0.05f)
        {
            transform->Set_Position(runtime->goalPos);
            transform->Set_RotationEuler(runtime->goalRot);
            continue;
        }

        path.Normalize();

        Vec3 toGoal = runtime->goalPos - currentPos;
        toGoal.y = 0.f;
        const float remain = toGoal.Length();

        if (remain <= 0.05f)
        {
            transform->Set_Position(runtime->goalPos);
            transform->Set_RotationEuler(runtime->goalRot);
            continue;
        }

        allReached = false;

        const float currentRatio = target.ModelCom->Get_RootMotionTranslationRatio();
        float deltaRatio = currentRatio - runtime->prevRootRatio;

        if (deltaRatio < 0.f)
            deltaRatio = 0.f;

        runtime->prevRootRatio = currentRatio;

        const float moveAmount = totalDistance * deltaRatio;
        if (moveAmount <= 0.f)
            continue;

        const float clampedMove = min(moveAmount, remain);
        transform->Set_Position(currentPos + path * clampedMove);

        Vec3 currentRot = transform->Get_RotationEuler();
        const Vec3 goalRot = runtime->goalRot;

        float deltaYaw = goalRot.y - currentRot.y;
        while (deltaYaw > 180.f)
            deltaYaw -= 360.f;
        while (deltaYaw < -180.f)
            deltaYaw += 360.f;

        const float turnSpeed = 180.f * timeDelta;
        if (fabsf(deltaYaw) <= turnSpeed)
            currentRot.y = goalRot.y;
        else
            currentRot.y += (deltaYaw > 0.f ? turnSpeed : -turnSpeed);

        transform->Set_RotationEuler(currentRot);
    }

    if (!allReached)
        return;

    for (const BattleChar& target : _reactionAnimTargets)
        TryRestoreTargetToSpawnPose(target);

    FinishReactionAnimation();
}

void BattleReactionController::StartCounterReturnAll(ModelCom* sourceModel)
{
    if (_animPhase != ReactionAnimPhase::Counter)
        return;

    if (sourceModel == nullptr)
        return;

    bool isCounterActor = false;
    for (const BattleChar& target : _reactionAnimTargets)
    {
        if (target.ModelCom.get() == sourceModel)
        {
            isCounterActor = true;
            break;
        }
    }

    if (!isCounterActor)
        return;

    StartCounterReturn();
}

void BattleReactionController::StartCounter(ModelCom* sourceModel)
{
    if (!_isCounterPending)
        return;

    for (const BattleChar& target : _reactionAnimTargets)
    {

        if (_reactionAnimTargets.size() >= 2)
        {
            // 위치 순간이동하고 카운터 시작 
            if (target.actor == nullptr || target.ModelCom == nullptr || _counterDamageTarget.actor == nullptr)
                continue;

            auto attackerTr = _counterDamageTarget.actor->Get_Transform();
            auto counterTr = target.actor->Get_Transform();
            if (attackerTr == nullptr || counterTr == nullptr)
                continue;

            Vec3 attackerPos = attackerTr->Get_Position();
            Vec3 counterPos = counterTr->Get_Position();

            Vec3 dir = attackerPos - counterPos;
            dir.y = 0.f;

            if (dir.LengthSquared() > 0.0001f)
            {
                dir.Normalize();

                Vec3 rot = counterTr->Get_RotationEuler();
                rot.y = XMConvertToDegrees(atan2f(dir.x, dir.z));
                counterTr->Set_RotationEuler(rot);

                Vec3 newPos = attackerPos - dir * 3.f;
                newPos.y = counterPos.y;
                counterTr->Set_Position(newPos);
            }

            EvtBattlePresentation event;
            event.cameraMode = BattleCameraMode::PARRY;
            event.subject = _reactionAnimTargets[_reactionAnimTargets.size() / 2].actor;
            event.presetId = "AoE";
            event.clipId = "AoE";
            GAME->Get_EventBus().Publish(event);
        }
        target.ModelCom->Set_RootMotionEnabled(true);

        if (!target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Counter), 0.12f, false))
            return;
    }

    _isCounterPending = false;
    _isCounterDamageApplied = false;
    _animPhase = ReactionAnimPhase::Counter;

    OnCounterStarted.Broadcast(_reactionAnimTargets);
}

void BattleReactionController::ApplyCounterDamage(ModelCom* sourceModel)
{
    UNREFERENCED_PARAMETER(sourceModel);

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

        DamageSystem::ApplyDamage(counterDamage, target, _counterDamageTarget, AttackType::Counter);
        ++hitCount;
    }
    // 단체 패링이여도 한번만 들어가게
    if (hitCount > 0)
    {
        PlayEffectDesc effectDesc{};
        effectDesc.effectName = L"Effects\\Assets\\Distortion\\Double_Distotion.effect.json";
        effectDesc.spawnBasisTarget = _counterDamageTarget.actor;
        effectDesc.scale = Vec3(1.2f, 1.2f, 1.2f);
        effectDesc.localOffset = Vec3(0.f, 1.2f, 0.f);

        CLIENT->Play_Effect(effectDesc);

        _isCounterDamageApplied = true;
    }
}

void BattleReactionController::BeginCounterTimeSection(ModelCom* sourceModel, float worldTimeScale,
    float counterFrameSpeed)
{
    if (!CanApplyCounterTimeSection(sourceModel))
        return;

    if (!_isCounterTimeSectionActive)
        _counterTimeSectionRestoreScale = GAME->Get_WorldTimeScale();

    _isCounterTimeSectionActive = true;

    for (const BattleChar& target : _reactionAnimTargets)
    {
        if (target.ModelCom.get() == sourceModel)
        {
            _counterTimeSectionModel = target.ModelCom;
            break;
        }
    }

    GAME->Set_WorldTimeScale(worldTimeScale);
    sourceModel->Set_FrameSpeed(counterFrameSpeed);
}

void BattleReactionController::EndCounterTimeSection(ModelCom* sourceModel)
{
    if (!_isCounterTimeSectionActive)
        return;

    if (_counterTimeSectionModel != nullptr && sourceModel != _counterTimeSectionModel.get())
        return;

    if (_counterTimeSectionModel)
        _counterTimeSectionModel->Set_FrameSpeed(1.f);

    GAME->Set_WorldTimeScale(_counterTimeSectionRestoreScale);

    _isCounterTimeSectionActive = false;
    _counterTimeSectionRestoreScale = 1.f;
    _counterTimeSectionModel = nullptr;
}

bool BattleReactionController::Is_GroupParryActive() const
{
    return _reactionAnimTargets.size() >= 2 && _animPhase == ReactionAnimPhase::Counter;
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
    return //animType == BattleAnimType::Hit_Heavy ||
        //animType == BattleAnimType::Hit_Medium ||
       // animType == BattleAnimType::Hit_Light ||
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

        EvtBattlePresentation event;
        //
        //if(_reactionAnimTargets.size() == 1)
        //{
        //    event.cameraMode = BattleCameraMode::PARRY;
        //    event.subject = _reactionAnimTargets.front().actor;
        //    event.presetId = magic_enum::enum_name(_reactionAnimTargets.front().heroId);
        //    event.clipId = magic_enum::enum_name(_reactionAnimTargets.front().heroId);
        //    event.useClip = true;
        //}
        //GAME->Get_EventBus().Publish(event);
        //
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

    const bool useRootMotion = (reactionType != BattleReactionType::Dodge);
    target.ModelCom->Set_RootMotionEnabled(useRootMotion);

    if (!target.ModelCom->Set_AnimationByState(ETOI(animType), 0.1f, false))
        return;

    for (const BattleChar& reactionTarget : _reactionAnimTargets)
    {
        if (reactionTarget.actor == target.actor)
            return;
    }

    _reactionAnimTargets.push_back(target);
    _animPhase = ReactionAnimPhase::Reaction;
}

void BattleReactionController::FinishReactionAnimation()
{
    ResetCounterTimeSection();

    const bool shouldRestoreSpawnPose = (_animPhase == ReactionAnimPhase::CounterReturn);

    for (const BattleChar& target : _reactionAnimTargets)
    {
        if (target.ModelCom == nullptr)
            continue;
        if (target.statCom->Get_AliveStatus() == EAliveStatus::Dead)
            continue;




        if (shouldRestoreSpawnPose)
            TryRestoreTargetToSpawnPose(target);

        target.ModelCom->Set_RootMotionEnabled(true);

        target.ModelCom->Set_FrameSpeed(1.f);
        target.ModelCom->Set_AnimationByState(
            ETOI(BattleAnimType::Base_Idle),
            0.15f,
            true);
    }

    _reactionAnimTargets.clear();
    _counterReturnRuntimes.clear();
    _counterDamageTarget = {};
    _animPhase = ReactionAnimPhase::None;
    _isCounterPending = false;
    _isCounterDamageApplied = false;
}

void BattleReactionController::TryRestoreTargetToSpawnPose(const BattleChar& target) const
{
    if (target.actor == nullptr)
        return;

    const auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    const Shared<BattleSpawnRegistry> spawnRegistry = battle->Get_BattleSpawnRegistry();
    if (spawnRegistry == nullptr)
        return;

    const BattleSpawnRegistry::BattleEntry* spawnEntry = spawnRegistry->Find_SpawnEntry(target.actor);
    if (spawnEntry == nullptr)
        return;

    const Shared<TransformCom> transform = target.actor->Get_Transform();
    if (transform == nullptr)
        return;

    transform->Set_Position(spawnEntry->spawnPosition);
    transform->Set_RotationEuler(spawnEntry->spawnRotation);
}

bool BattleReactionController::CanApplyCounterTimeSection(ModelCom* sourceModel) const
{
    if (_animPhase != ReactionAnimPhase::Counter || sourceModel == nullptr)
        return false;

    for (const BattleChar& target : _reactionAnimTargets)
    {
        if (target.ModelCom.get() == sourceModel)
            return true;
    }

    return false;
}

void BattleReactionController::ResetCounterTimeSection()
{
    if (_counterTimeSectionModel)
        _counterTimeSectionModel->Set_FrameSpeed(1.f);

    if (_isCounterTimeSectionActive)
        GAME->Set_WorldTimeScale(_counterTimeSectionRestoreScale);

    _isCounterTimeSectionActive = false;
    _counterTimeSectionRestoreScale = 1.f;
    _counterTimeSectionModel = nullptr;
}

Shared<BattleReactionController> BattleReactionController::Create()
{
    return make_shared<BattleReactionController>();
}

void BattleReactionController::Free()
{
    OnReactionWindowOpened.Clear();
    OnReactionSucceeded.Clear();
    OnCounterStarted.Clear();

    Clear_RuntimeState();
    Base::Free();
}
