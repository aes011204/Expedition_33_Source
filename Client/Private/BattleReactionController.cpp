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
#include "EffectCom.h"

#include <algorithm>

namespace
{
    constexpr uint32 kReactionConsumeKeyStride = 16;

    size_t ReactionIndex(BattleReactionType type)
    {
        return static_cast<size_t>(ETOI(type));
    }

    bool IsWindowManagedReaction(BattleReactionType type)
    {
        return type == BattleReactionType::Dodge ||
            type == BattleReactionType::Parry ||
            type == BattleReactionType::Jump ||
            type == BattleReactionType::JumpCounter;
    }

    void Play_TargetOwnerEffect(
        const BattleChar& target,
        const wstring& effectName,
        const Vec3& localOffset,
        const Vec3& scale)
    {
        if (target.actor == nullptr || effectName.empty())
            return;

        const Shared<EffectCom> effectCom = target.actor->Get_Component<EffectCom>();
        if (effectCom == nullptr)
            return;

        EffectCom::EffectPlayDesc effectDesc{};
        effectDesc.levelIndex = GAME->Current_LevelIndex();
        effectDesc.effectName = effectName;
        effectDesc.target = EffectCom::EffectPlayTarget::OwnerRoot;
        effectDesc.localOffset = localOffset;
        effectDesc.scale = scale;
        effectDesc.followTarget = false;

        effectCom->Play_Effect(effectDesc);
    }
}

uint32 BattleReactionController::Make_ReactionConsumeKey(uint32 sourceStepIndex, uint32 echoLaneIndex)
{
    if (sourceStepIndex == UINT32_MAX)
        return UINT32_MAX;

    return sourceStepIndex * kReactionConsumeKeyStride + echoLaneIndex;
}

void BattleReactionController::OpenWindow(BattleReactionType type)
{
    if (!IsWindowManagedReaction(type))
        return;

    const size_t index = ReactionIndex(type);

    ++_reactionWindowOpenCounts[index];

    OnReactionWindowOpened.Broadcast(type);
}

void BattleReactionController::OpenWindow(BattleReactionType type, const BattleAction& turnAction, float durationSec)
{
    if (!IsWindowManagedReaction(type))
        return;

    if (turnAction.GetCurStepIndex() >= turnAction.GetSteps().size())
        return;

    OpenWindow(type);

    const size_t index = ReactionIndex(type);
    _reactionWindowElapsedSec[index] = 0.f;
    _reactionWindowRemainingSec[index] = max(0.f, durationSec);
    _reactionWindowContexts[index] = BuildReactionInputContext(type, turnAction);
}

void BattleReactionController::CloseWindow(BattleReactionType type)
{
    if (!IsWindowManagedReaction(type))
        return;

    const size_t index = ReactionIndex(type);
    if (_reactionWindowOpenCounts[index] == 0)
        return;

    --_reactionWindowOpenCounts[index];

    if (_reactionWindowOpenCounts[index] > 0)
        return;

    _reactionWindowElapsedSec[index] = 0.f;
    _reactionWindowRemainingSec[index] = 0.f;
    _reactionWindowContexts[index] = {};

    OnReactionWindowClosed.Broadcast(type);
}

void BattleReactionController::Clear()
{
    const bool keepJumpCounterWindow =
        _jumpCounterState == JumpCounterState::CounterWindowOpen;

    // 초기화
    _reactionWindowOpenCounts.fill(0);
    _reactionWindowElapsedSec.fill(0.f);
    _reactionWindowRemainingSec.fill(0.f);
    _reactionWindowContexts.fill({});

    if (keepJumpCounterWindow)
        _reactionWindowOpenCounts[ReactionIndex(BattleReactionType::JumpCounter)] = 1;

    ClearConsumedInput();
    Clear_ParryCounterRuntime();
}

void BattleReactionController::Clear_RuntimeState()
{
    Clear();
    Clear_EchoReactionWindows();
    ResetCounterTimeSection();

    for (const BattleChar& target : _reactionAnimTargets)
    {
        if (target.ModelCom)
            target.ModelCom->Set_FrameSpeed(1.f);
    }

    _reactionAnimTargets.clear();
    _counterReturnRuntimes.clear();
    _counterDamageTarget = {};
    _pendingParryCounterTarget = {};
    _animPhase = ReactionAnimPhase::None;
    _isCounterPending = false;
    _isCounterDamageApplied = false;
    _reactionAnimElapsed = 0.f;

    ResetJumpCounterSequence();
}

bool BattleReactionController::TryInput(BattleReactionType type, uint32 consumeKey)
{
    if (Is_ConsumeKeyPending(consumeKey))
        return false;

    if (!IsWindowOpen(type))
        return false;

    _consumedReactionInputs.push_back({ type, consumeKey });
    _result = type;

    if (type == BattleReactionType::Jump)
    {
        LOG_INFO("Jump Time Scale");
        GAME->Set_WorldTimeScale(0.15f);
    }

    return true;
}

bool BattleReactionController::TryConsume(BattleReactionType& outResult, uint32 consumeKey)
{
    for (size_t i = 0; i < _consumedReactionInputs.size();)
    {
        const ConsumedReactionInput& input = _consumedReactionInputs[i];

        if (input.consumeKey == consumeKey)
        {
            outResult = input.type;
            _consumedReactionInputs.erase(_consumedReactionInputs.begin() + i);
            if (_consumedReactionInputs.empty())
                _result = BattleReactionType::None;

            return outResult == BattleReactionType::Dodge ||
                outResult == BattleReactionType::Parry ||
                outResult == BattleReactionType::Jump ||
                outResult == BattleReactionType::JumpCounter;
        }

        // 현재 데미지보다 앞선 입력은 더 이상 소비될 수 없으므로 버린다.
        if (input.consumeKey < consumeKey)
        {
            _consumedReactionInputs.erase(_consumedReactionInputs.begin() + i);
            continue;
        }

        ++i;
    }

    if (_consumedReactionInputs.empty())
        _result = BattleReactionType::None;

    return false;
}

void BattleReactionController::ClearConsumedInput()
{
    _consumedReactionInputs.clear();
    _result = BattleReactionType::None;
}

bool BattleReactionController::Is_ConsumeKeyPending(uint32 consumeKey) const
{
    for (const ConsumedReactionInput& input : _consumedReactionInputs)
    {
        if (input.consumeKey == consumeKey)
            return true;
    }

    return false;
}

void BattleReactionController::ResetJumpCounterSequence()
{
    const size_t index = ReactionIndex(BattleReactionType::JumpCounter);
    const bool wasOpen = _reactionWindowOpenCounts[index] > 0;

    _reactionWindowOpenCounts[index] = 0;
    _reactionWindowElapsedSec[index] = 0.f;
    _reactionWindowRemainingSec[index] = 0.f;
    _reactionWindowContexts[index] = {};

    if (wasOpen)
        OnReactionWindowClosed.Broadcast(BattleReactionType::JumpCounter);

    _jumpCounterState = JumpCounterState::None;
    _jumpCounterStepIndex = UINT32_MAX;
    _jumpCounterTarget = {};
    _jumpCounterStarted = false;
}

void BattleReactionController::NotifyJumpCounterWindowBegin(float worldTimeScale)
{
    if (_jumpCounterState != JumpCounterState::JumpSucceeded)
        return;

    _jumpCounterState = JumpCounterState::CounterWindowOpen;
    OpenWindow(BattleReactionType::JumpCounter);
    GAME->Set_WorldTimeScale(clamp(worldTimeScale, 0.01f, 1.f));
}

void BattleReactionController::NotifyJumpCounterWindowEnd()
{
    CloseWindow(BattleReactionType::JumpCounter);

    if (_jumpCounterState == JumpCounterState::CounterWindowOpen)
    {
        GAME->Set_WorldTimeScale(1.f);
        ResetJumpCounterSequence();
    }
}

bool BattleReactionController::TryStartQueuedJumpCounter(ModelCom* sourceModel)
{
    if (_jumpCounterState != JumpCounterState::CounterSucceeded || _jumpCounterStarted)
        return false;

    GAME->Set_WorldTimeScale(1.f);

    StartCounter(sourceModel);

    if (_jumpCounterState == JumpCounterState::Started)
    {
        _jumpCounterStarted = true;
        return true;
    }

    return false;
}

bool BattleReactionController::Try_GetJumpCounterTarget(BattleChar& outTarget) const
{
    if (_jumpCounterTarget.actor == nullptr || _jumpCounterTarget.statCom == nullptr)
        return false;

    outTarget = _jumpCounterTarget;
    return true;
}

bool BattleReactionController::TryConsumeGuard(
    const BattleChar& target,
    uint32 stepIndex,
    uint32 consumeKey,
    BattleReactionType* outConsumedReaction)
{
    if (outConsumedReaction != nullptr)
        *outConsumedReaction = BattleReactionType::None;

    if (target.actor == nullptr || target.statCom == nullptr)
        return false;

    if (!target.statCom->Is_Player())
        return false;

    BattleReactionType result = BattleReactionType::None;
    const uint32 resolvedConsumeKey =
        consumeKey == UINT32_MAX ? Make_ReactionConsumeKey(stepIndex) : consumeKey;

    if (!TryConsume(result, resolvedConsumeKey))
        return false;

    if (result == BattleReactionType::Jump ||
        result == BattleReactionType::JumpCounter)
    {
        if (_curSkillData == nullptr)
            return false;

        if (stepIndex >= _curSkillData->targetingType.size())
            return false;

        const TargetType currentTargetType = _curSkillData->targetingType[stepIndex];
        return currentTargetType == TargetType::PrimaryJump ||
            currentTargetType == TargetType::AllEnemiesJump;
    }

    const bool guarded =
        result == BattleReactionType::Dodge ||
        result == BattleReactionType::Parry;

    if (guarded && outConsumedReaction != nullptr)
        *outConsumedReaction = result;

    if (guarded && result == BattleReactionType::Parry)
    {
        Mark_ParryCounterGuardedKey(stepIndex, resolvedConsumeKey);
        Try_ArmParryCounterAfterGuard(target, stepIndex);
    }

    return guarded;
}

bool BattleReactionController::IsWindowOpen(BattleReactionType type) const
{
    if (!IsWindowManagedReaction(type))
        return false;

    return _reactionWindowOpenCounts[ReactionIndex(type)] > 0;
}

BattleReactionController::BattleReactionInputResult BattleReactionController::SubmitInput(BattleReactionType type,
    const BattleAction& turnAction)
{
    BattleReactionInputResult result{};

    if (_curSkillData == nullptr)
        return result;

    if (turnAction.GetSteps().size() <= turnAction.GetCurStepIndex())
        return result;

    ReactionInputContext context = BuildReactionInputContext(type, turnAction);
    if (context.targets.empty())
        return result;

    return SubmitInput(context);
}

BattleReactionController::BattleReactionInputResult BattleReactionController::SubmitInput(const ReactionInputContext& context)
{
    BattleReactionInputResult result{};

    if (_curSkillData == nullptr)
        return result;

    if (context.targets.empty())
        return result;

    if (context.type != BattleReactionType::JumpCounter && Is_ConsumeKeyPending(context.consumeKey))
        return result;

    vector<BattleChar> inputTargets{};
    if (context.type == BattleReactionType::JumpCounter)
    {
        inputTargets = context.targets;
    }
    else
    {
        for (const BattleChar& target : context.targets)
        {
            if (CanReceiveInput(target, context.type))
                inputTargets.push_back(target);
        }

        if (inputTargets.empty())
            return result;
    }

    const bool allowCounterOnSuccess =
        context.allowCounterOnSuccess &&
        (context.isEcho || !Is_CounterSuppressedForSourceStep(context.stepIndex));

    if (context.type == BattleReactionType::Dodge)
    {
        const bool success = TryInput(BattleReactionType::Dodge, context.consumeKey);

        result.hasInput = true;
        result.type = BattleReactionType::Dodge;
        result.success = success;

        if (success && context.isEcho)
            Mark_EchoReactionInputConsumed(context);

        for (const BattleChar& target : inputTargets)
        {
            PlayReactionAnimation(
                target,
                BattleReactionType::Dodge,
                success,
                {},
                context.stepIndex,
                false);
        }
    }
    else if (context.type == BattleReactionType::Parry)
    {
        const bool success = TryInput(BattleReactionType::Parry, context.consumeKey);

        result.hasInput = true;
        result.type = BattleReactionType::Parry;
        result.success = success;

        if (success && context.isEcho)
            Mark_EchoReactionInputConsumed(context);

        if (success)
        {
            _reactionAnimTargets.clear();
            _counterReturnRuntimes.clear();
            _counterDamageTarget = {};
            _isCounterPending = false;
            _isCounterDamageApplied = false;
            _animPhase = ReactionAnimPhase::None;

            if (allowCounterOnSuccess)
                _pendingParryCounterTarget = context.counterTarget;

            // 단체 패링이어도 HitStop, Sound는 한번만 재생
            if (auto battle = CLIENT->Get_BattleController())
                battle->Play_BattleHitStop(0.08f, 0.08f);

			GAME->Play_SoundCue(Constants::SoundCue::Battle_Parry_Success);
        }

        for (const BattleChar& target : inputTargets)
        {
            PlayReactionAnimation(
                target,
                BattleReactionType::Parry,
                success,
                context.counterTarget,
                context.stepIndex,
                allowCounterOnSuccess);

            if (success && target.actor != nullptr)
            {
                Play_TargetOwnerEffect(
                    target,
                    L"Effects\\Assets\\Player\\Player_Counter.effect.json",
                    Vec3(0.f, 0.9f, 0.f),
                    Vec3(0.12f, 0.12f, 0.12f)); // 얼마나 줄여야하는겨;
            }

            if (success &&
                target.actor != nullptr &&
                target.statCom != nullptr &&
                target.statCom->Is_Player())
            {
                result.apRewardTargets.push_back(target);
                result.apRewardAmount = 1;
                result.apRewardReason = EAPChangeReason::Parry;
            }
        }
    }
    else if (context.type == BattleReactionType::Jump)
    {
        const bool success = TryInput(BattleReactionType::Jump, context.consumeKey);

        result.hasInput = true;
        result.type = BattleReactionType::Jump;
        result.success = success;

        if (success)
        {
            _jumpCounterState = JumpCounterState::JumpSucceeded;
            _jumpCounterStepIndex = context.stepIndex;
            _jumpCounterTarget = context.counterTarget;
            _jumpCounterStarted = false;
            LOG_INFO("JUMP success step={}", context.stepIndex);
        }
        else
        {
            ResetJumpCounterSequence();
        }

        for (const BattleChar& target : inputTargets)
        {
            PlayReactionAnimation(
                target,
                BattleReactionType::Jump,
                success,
                {},
                context.stepIndex,
                false);
        }
    }
    else if (context.type == BattleReactionType::JumpCounter)
    {
        result.hasInput = true;
        result.type = BattleReactionType::JumpCounter;

        if (_jumpCounterState != JumpCounterState::CounterWindowOpen ||
            _jumpCounterStepIndex != context.stepIndex ||
            !IsWindowOpen(BattleReactionType::JumpCounter))
        {
            return result;
        }

        result.success = true;
        _jumpCounterState = JumpCounterState::CounterSucceeded;
        _counterDamageTarget = _jumpCounterTarget;
        _isCounterPending = true;
        _isCounterDamageApplied = false;
        _jumpCounterStarted = false;
        GAME->Set_WorldTimeScale(1.f);
        CloseWindow(BattleReactionType::JumpCounter);
        LOG_INFO("JUMPCOUNTER success step={}", context.stepIndex);
    }

    if (result.success)
        OnReactionSucceeded.Broadcast(result.type, inputTargets);

    return result;
}

void BattleReactionController::Notify_TargetDamaged(const BattleChar& target)
{
    if (target.actor == nullptr || target.statCom == nullptr)
        return;

    if (!target.statCom->Is_Player())
        return;

    for (auto it = _reactionAnimTargets.begin(); it != _reactionAnimTargets.end(); )
    {
        if (it->actor == target.actor)
            it = _reactionAnimTargets.erase(it);
        else
            ++it;
    }

    if (_counterDamageTarget.actor == target.actor)
        _counterDamageTarget = {};

    for (auto it = _counterReturnRuntimes.begin(); it != _counterReturnRuntimes.end(); )
    {
        if (it->target.actor == target.actor)
            it = _counterReturnRuntimes.erase(it); 
        else
            ++it;
    }

    if (!_reactionAnimTargets.empty())
        return;

    ResetCounterTimeSection();

    _counterReturnRuntimes.clear();
    _counterDamageTarget = {};
    Clear_ParryCounterRuntime();
    _animPhase = ReactionAnimPhase::None;
    _isCounterPending = false;
    _isCounterDamageApplied = false;
    ResetJumpCounterSequence();
}

BattleReactionController::BattleReactionInputResult BattleReactionController::UpdateInput(
    const BattleAction& turnAction,
    bool autoParryEnabled)
{
    if (GAME->KeyDown(KEY_TYPE::LBUTTON))
        return SubmitInput(BattleReactionType::JumpCounter, turnAction);

    if (GAME->KeyDown(KEY_TYPE::Q))
    {
        if (const ReactionInputContext* echoContext = Find_ActiveEchoReactionContext(BattleReactionType::Dodge))
            return SubmitInput(*echoContext);

        return SubmitInput(BattleReactionType::Dodge, turnAction);
    }

    if (GAME->KeyDown(KEY_TYPE::E))
    {
        if (const ReactionInputContext* echoContext = Find_ActiveEchoReactionContext(BattleReactionType::Parry))
            return SubmitInput(*echoContext);

        return SubmitInput(BattleReactionType::Parry, turnAction);
    }

    if (autoParryEnabled &&
        turnAction.Get_CurChar().statCom != nullptr &&
        !turnAction.Get_CurChar().statCom->Is_Player())
    {
        if (const ReactionInputContext* echoContext = Find_ActiveEchoReactionContext(BattleReactionType::Parry))
            return SubmitInput(*echoContext);

        if (IsWindowOpen(BattleReactionType::Parry))
            return SubmitInput(BattleReactionType::Parry, turnAction);

        if (IsWindowOpen(BattleReactionType::JumpCounter))
            return SubmitInput(BattleReactionType::JumpCounter, turnAction);

        if (IsWindowOpen(BattleReactionType::Jump))
            return SubmitInput(BattleReactionType::Jump, turnAction);
    }

    if (GAME->KeyDown(KEY_TYPE::SPACE))
        return SubmitInput(BattleReactionType::Jump, turnAction);

    return {};
}

void BattleReactionController::UpdateAnimation(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (_reactionAnimTargets.empty())
        return;

    _reactionAnimElapsed += std::max(0.f, GAME->Get_UnscaledTimeDelta());

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

        const float currentRatio = target.ModelCom->Get_RootMotionTranslationRatio();

        if (remain <= 0.05f)
        {
            transform->Set_Position(runtime->goalPos);
            transform->Set_RotationEuler(runtime->goalRot);
            runtime->prevRootRatio = currentRatio;
            continue;
        }

        if (currentRatio >= 0.999f || target.ModelCom->Is_AnimationFinished())
        {
            // 복귀 애니메이션이 끝났으면 남은 오차를 스폰 위치로 정리하고 busy를 풀어 턴 종료를 이어간다.
            transform->Set_Position(runtime->goalPos);
            transform->Set_RotationEuler(runtime->goalRot);
            runtime->prevRootRatio = currentRatio;
            continue;
        }

        allReached = false;

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

void BattleReactionController::Queue_EchoReactionWindow(float delaySec, float durationSec,
    const ReactionInputContext& context)
{
    if (context.type != BattleReactionType::Dodge &&
        context.type != BattleReactionType::Parry)
        return;

    if (context.targets.empty())
        return;

    DelayedEchoReactionWindow window{};
    window.delaySec = max(0.f, delaySec);
    window.durationSec = max(0.f, durationSec);
    window.context = context;
    window.context.isEcho = true;

    _delayedEchoReactionWindows.push_back(window);

    if (window.context.type == BattleReactionType::Parry)
        Register_ParryCounterRequiredKey(window.context.stepIndex, window.context.consumeKey);
}

vector<BattleReactionController::ActiveReactionWindowSnapshot> BattleReactionController::
        Get_ActiveReactionWindowSnapshots() const
{
    vector<ActiveReactionWindowSnapshot> snapshots{};

    for (BattleReactionType type : { BattleReactionType::Dodge, BattleReactionType::Parry })
    {
        const size_t index = ReactionIndex(type);
        if (_reactionWindowOpenCounts[index] == 0)
            continue;

        const ReactionInputContext& context = _reactionWindowContexts[index];
        if (context.targets.empty())
            continue;

        ActiveReactionWindowSnapshot snapshot{};
        snapshot.type = type;
        snapshot.elapsedSec = _reactionWindowElapsedSec[index];
        snapshot.remainingSec = _reactionWindowRemainingSec[index];
        snapshot.context = context;
        snapshots.push_back(snapshot);
    }

    return snapshots;
}

void BattleReactionController::Update_EchoReactionWindows(float timeDelta)
{
    const float dt = max(0.f, timeDelta);

    for (BattleReactionType type : { BattleReactionType::Dodge, BattleReactionType::Parry })
    {
        const size_t index = ReactionIndex(type);
        if (_reactionWindowOpenCounts[index] == 0)
            continue;

        _reactionWindowElapsedSec[index] += dt;
        if (_reactionWindowRemainingSec[index] > 0.f)
            _reactionWindowRemainingSec[index] = max(0.f, _reactionWindowRemainingSec[index] - dt);
    }

    for (size_t i = 0; i < _delayedEchoReactionWindows.size();)
    {
        DelayedEchoReactionWindow& window = _delayedEchoReactionWindows[i];

        if (!window.opened)
        {
            window.delaySec -= dt;

            if (window.delaySec <= 0.f)
            {
                window.opened = true;
                OpenWindow(window.context.type);
            }
        }

        if (window.opened)
        {
            if (window.inputConsumed)
            {
                _delayedEchoReactionWindows.erase(_delayedEchoReactionWindows.begin() + i);
                continue;
            }

            window.durationSec -= dt;

            if (window.durationSec <= 0.f)
            {
                CloseWindow(window.context.type);
                _delayedEchoReactionWindows.erase(_delayedEchoReactionWindows.begin() + i);
                continue;
            }
        }

        ++i;
    }
}

void BattleReactionController::Clear_EchoReactionWindows()
{
    for (const DelayedEchoReactionWindow& window : _delayedEchoReactionWindows)
    {
        if (window.opened)
            CloseWindow(window.context.type);
    }

    _delayedEchoReactionWindows.clear();
    _echoCounterSuppressedSteps.clear();
    Clear_ParryCounterRuntime();
}

void BattleReactionController::Suppress_CounterForSourceStep(uint32 stepIndex)
{
    _echoCounterSuppressedSteps.insert(stepIndex);
    Register_ParryCounterRequiredKey(stepIndex, Make_ReactionConsumeKey(stepIndex));
}

void BattleReactionController::StartCounter(ModelCom* sourceModel)
{
    if (!_isCounterPending)
        return;

    const BattleAnimType counterAnimType =
        _jumpCounterState == JumpCounterState::CounterSucceeded ||
        _jumpCounterState == JumpCounterState::Started
        ? BattleAnimType::JumpCounter
        : BattleAnimType::Counter;

	const bool isGroupCounter =
		counterAnimType == BattleAnimType::Counter &&
		_reactionAnimTargets.size() >= 2;

	// 단체 카운터 시에는, 애니메이션 변경
	  const BattleAnimType resolvedCounterAnimType =
        isGroupCounter
        ? BattleAnimType::GroupCounter
        : counterAnimType;

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
            event.useClip = true;
            GAME->Get_EventBus().Publish(event);
        }
        target.ModelCom->Set_RootMotionEnabled(true);

        if (!target.ModelCom->Set_AnimationByState(ETOI(resolvedCounterAnimType), 0.12f, false))
            return;
    }

    _isCounterPending = false;
    _isCounterDamageApplied = false;
    _animPhase = ReactionAnimPhase::Counter;
    if (counterAnimType == BattleAnimType::JumpCounter)
        _jumpCounterState = JumpCounterState::Started;

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

    uint32 hitCount = 0;

    for (const BattleChar& target : _reactionAnimTargets)
    {
        if (target.actor == nullptr || target.statCom == nullptr)
            continue;

        const float counterDamage =
            DamageSystem::CalculatePlayerAttackDamage(target, 1.f);

        DamageSystem::ApplyDamage(counterDamage, target, _counterDamageTarget, AttackType::Counter);
        ++hitCount;
    }

    // 단체 카운터여도 이팩트는 한번만 들어가게
    if (hitCount > 0)
    {
        Play_TargetOwnerEffect(
            _counterDamageTarget,
            L"Effects\\Assets\\Player\\Player_Counter.effect.json",
            Vec3(0.f, 1.2f, 0.f),
            Vec3(1.f, 1.f, 1.f));

        _isCounterDamageApplied = true;
    }
}

void BattleReactionController::BeginCounterTimeSection(ModelCom* sourceModel, float worldTimeScale,
    float counterFrameSpeed)
{
    if (!CanApplyCounterTimeSection(sourceModel))
        return;

    if (std::find_if(_counterTimeSectionModels.begin(), _counterTimeSectionModels.end(),
        [sourceModel](const Shared<ModelCom>& model)
        {
            return model.get() == sourceModel;
        }) != _counterTimeSectionModels.end())
    {
        return;
    }

    Shared<ModelCom> sectionModel = nullptr;
    for (const BattleChar& target : _reactionAnimTargets)
    {
        if (target.ModelCom.get() == sourceModel)
        {
            sectionModel = target.ModelCom;
            break;
        }
    }

    if (sectionModel == nullptr)
        return;

    if (!_isCounterTimeSectionActive)
    {
        _counterTimeSectionRestoreScale = GAME->Get_WorldTimeScale();
        GAME->Set_WorldTimeScale(worldTimeScale); // 단체 패링에서는 첫 notify만 월드 배속을 잡는다.
    }

    _isCounterTimeSectionActive = true;
    _counterTimeSectionModels.push_back(sectionModel);
    sourceModel->Set_FrameSpeed(counterFrameSpeed);
}

void BattleReactionController::EndCounterTimeSection(ModelCom* sourceModel)
{
    if (!_isCounterTimeSectionActive)
        return;

    auto it = std::find_if(_counterTimeSectionModels.begin(), _counterTimeSectionModels.end(),
        [sourceModel](const Shared<ModelCom>& model)
        {
            return model.get() == sourceModel;
        });

    if (it == _counterTimeSectionModels.end())
        return;

    if (*it)
        (*it)->Set_FrameSpeed(1.f);

    _counterTimeSectionModels.erase(it);

    if (!_counterTimeSectionModels.empty())
        return;

    GAME->Set_WorldTimeScale(_counterTimeSectionRestoreScale);
    _isCounterTimeSectionActive = false;
    _counterTimeSectionRestoreScale = 1.f;
}

bool BattleReactionController::Is_GroupParryActive() const
{
    return _reactionAnimTargets.size() >= 2 && _animPhase == ReactionAnimPhase::Counter;
}

bool BattleReactionController::CanReceiveInput(const BattleChar& target, BattleReactionType reactionType) const
{
    if (target.actor == nullptr || target.statCom == nullptr || target.ModelCom == nullptr)
        return false;

    if (!target.statCom->Is_Player())
        return false;

    for (const BattleChar& reactionTarget : _reactionAnimTargets)
    {
        if (reactionTarget.actor == target.actor &&
            reactionTarget.ModelCom != nullptr &&
            !reactionTarget.ModelCom->Is_AnimationFinished())
        {
            const BattleAnimType currentAnimType =
                static_cast<BattleAnimType>(reactionTarget.ModelCom->Get_AnimationStateIndex());
            const bool isRepeatableInput =
                reactionType == BattleReactionType::Dodge ||
                reactionType == BattleReactionType::Parry ||
                reactionType == BattleReactionType::Jump;
            const bool isRepeatableAnim =
                currentAnimType == BattleAnimType::Dodge ||
                currentAnimType == BattleAnimType::Parray ||
                (currentAnimType == BattleAnimType::JumpCounter &&
                    reactionType == BattleReactionType::Parry);

            if (!isRepeatableInput || !isRepeatableAnim)
            {
                return false;
            }
        }
    }

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
        animType == BattleAnimType::Hit_Stun ||
        animType == BattleAnimType::Dead;
}

void BattleReactionController::PlayReactionAnimation(const BattleChar& target, BattleReactionType reactionType,
    bool isSuccess, const BattleChar& counterTarget, uint32 curAttackIndex, bool allowCounterOnSuccess)
{
    UNREFERENCED_PARAMETER(curAttackIndex);
    UNREFERENCED_PARAMETER(allowCounterOnSuccess);

    if (target.ModelCom == nullptr)
        return;

    if (target.statCom->Get_AliveStatus() == EAliveStatus::Dead)
        return;

    BattleAnimType animType = BattleAnimType::Base_Idle;

    if (reactionType == BattleReactionType::Dodge)
    {
        animType = BattleAnimType::Dodge;
    }
    else if (reactionType == BattleReactionType::Jump)
    {
        animType = BattleAnimType::Jump;
    }
    else if (reactionType == BattleReactionType::Parry)
    {
        animType = BattleAnimType::Parray; // 성공하면 패링 애니메이션
    }
    else if (reactionType == BattleReactionType::JumpCounter)
    {
        animType = BattleAnimType::JumpCounter;

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

    const bool useRootMotion =
        reactionType != BattleReactionType::Dodge &&
        reactionType != BattleReactionType::Jump;
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
    _reactionAnimElapsed = 0.f;
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

        const auto battle = CLIENT->Get_BattleController();
        const bool shouldReturnToTargetedIdle =
            battle != nullptr &&
            target.statCom != nullptr &&
            target.statCom->Is_Player() &&
            battle->Get_TurnAction().Get_CurChar().statCom != nullptr &&
            !battle->Get_TurnAction().Get_CurChar().statCom->Is_Player();

        if (shouldReturnToTargetedIdle &&
            target.ModelCom->Set_AnimationByState(
                ETOI(BattleAnimType::Targeted_Idle),
                0.15f,
                true))
        {
            continue;
        }

        target.ModelCom->Set_AnimationByState(
            ETOI(BattleAnimType::Base_Idle),
            0.15f,
            true);
    }

    _reactionAnimTargets.clear();
    _counterReturnRuntimes.clear();
    _counterDamageTarget = {};
    Clear_ParryCounterRuntime();
    _animPhase = ReactionAnimPhase::None;
    _isCounterPending = false;
    _isCounterDamageApplied = false;
    _reactionAnimElapsed = 0.f;
    GAME->Set_WorldTimeScale(1.f);
    ResetJumpCounterSequence();
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
    for (const Shared<ModelCom>& model : _counterTimeSectionModels)
    {
        if (model)
            model->Set_FrameSpeed(1.f);
    }

    if (_isCounterTimeSectionActive)
        GAME->Set_WorldTimeScale(_counterTimeSectionRestoreScale);

    _isCounterTimeSectionActive = false;
    _counterTimeSectionRestoreScale = 1.f;
    _counterTimeSectionModels.clear();
}

BattleReactionController::ReactionInputContext BattleReactionController::BuildReactionInputContext(
    BattleReactionType type, const BattleAction& turnAction) const
{
    ReactionInputContext context{};
    context.type = type;
    context.stepIndex =
        type == BattleReactionType::JumpCounter && _jumpCounterStepIndex != UINT32_MAX
        ? _jumpCounterStepIndex
        : turnAction.GetCurStepIndex();
    context.consumeKey = Make_ReactionConsumeKey(context.stepIndex);
    context.targets =
        type == BattleReactionType::JumpCounter && !_reactionAnimTargets.empty()
        ? _reactionAnimTargets
        : turnAction.GetCurStepTarget();
    context.counterTarget =
        type == BattleReactionType::JumpCounter && _jumpCounterTarget.actor != nullptr
        ? _jumpCounterTarget
        : turnAction.Get_CurChar();

    context.allowCounterOnSuccess =
        type == BattleReactionType::Parry &&
        Is_LastSkillAttackStep(context.stepIndex) &&
        !Is_CounterSuppressedForSourceStep(context.stepIndex);

    context.isEcho = false;

    return context;
}

const BattleReactionController::ReactionInputContext* BattleReactionController::Find_ActiveEchoReactionContext(
    BattleReactionType type) const
{
    for (const DelayedEchoReactionWindow& window : _delayedEchoReactionWindows)
    {
        if (!window.opened)
            continue;

        if (window.inputConsumed)
            continue;

        if (window.context.type == type)
            return &window.context;
    }

    return nullptr;
}

void BattleReactionController::Mark_EchoReactionInputConsumed(const ReactionInputContext& context)
{
    for (DelayedEchoReactionWindow& window : _delayedEchoReactionWindows)
    {
        if (!window.opened || window.inputConsumed)
            continue;

        if (window.context.type != context.type ||
            window.context.consumeKey != context.consumeKey)
            continue;

        window.inputConsumed = true;
        CloseWindow(window.context.type);
        return;
    }
}

bool BattleReactionController::Is_LastSkillAttackStep(uint32 stepIndex) const
{
    return _curSkillData != nullptr &&
        !_curSkillData->targetingType.empty() &&
        stepIndex + 1 >= _curSkillData->targetingType.size();
}

bool BattleReactionController::Is_CounterSuppressedForSourceStep(uint32 stepIndex) const
{
    return _echoCounterSuppressedSteps.find(stepIndex) != _echoCounterSuppressedSteps.end();
}

void BattleReactionController::Register_ParryCounterRequiredKey(uint32 stepIndex, uint32 consumeKey)
{
    if (stepIndex == UINT32_MAX || consumeKey == UINT32_MAX)
        return;

    _parryCounterRequiredConsumeKeys[stepIndex].insert(consumeKey);
}

void BattleReactionController::Mark_ParryCounterGuardedKey(uint32 stepIndex, uint32 consumeKey)
{
    if (stepIndex == UINT32_MAX || consumeKey == UINT32_MAX)
        return;

    _parryCounterGuardedConsumeKeys[stepIndex].insert(consumeKey);

    const auto requiredIt = _parryCounterRequiredConsumeKeys.find(stepIndex);
    const size_t requiredCount =
        requiredIt != _parryCounterRequiredConsumeKeys.end() ? requiredIt->second.size() : 1;
    const size_t guardedCount = _parryCounterGuardedConsumeKeys[stepIndex].size();

    LOG_INFO("PARRY guarded step={} count={}/{}", stepIndex, guardedCount, requiredCount);
}

bool BattleReactionController::Is_ParryCounterGuardChainComplete(uint32 stepIndex) const
{
    const auto requiredIt = _parryCounterRequiredConsumeKeys.find(stepIndex);
    if (requiredIt == _parryCounterRequiredConsumeKeys.end())
        return true;

    const auto guardedIt = _parryCounterGuardedConsumeKeys.find(stepIndex);
    if (guardedIt == _parryCounterGuardedConsumeKeys.end())
        return false;

    for (const uint32 requiredKey : requiredIt->second)
    {
        if (guardedIt->second.find(requiredKey) == guardedIt->second.end())
            return false;
    }

    return true;
}

void BattleReactionController::Try_ArmParryCounterAfterGuard(const BattleChar& target, uint32 stepIndex)
{
    if (!Is_LastSkillAttackStep(stepIndex))
        return;

    if (_pendingParryCounterTarget.actor == nullptr ||
        _pendingParryCounterTarget.statCom == nullptr)
        return;

    if (!Is_ParryCounterGuardChainComplete(stepIndex))
        return;

    _isCounterPending = true;
    _counterDamageTarget = _pendingParryCounterTarget;
    _isCounterDamageApplied = false;

    EvtBattlePresentation event;
    event.cameraMode = BattleCameraMode::PARRY;
    event.subject = target.actor;
    event.presetId = magic_enum::enum_name(target.heroId);
    event.clipId = magic_enum::enum_name(target.heroId);
    event.useClip = true;

    GAME->Get_EventBus().Publish(event);

    LOG_INFO("PARRY counter armed step={}", stepIndex);

    _pendingParryCounterTarget = {};
    _parryCounterRequiredConsumeKeys.erase(stepIndex);
    _parryCounterGuardedConsumeKeys.erase(stepIndex);
}

void BattleReactionController::Clear_ParryCounterRuntime()
{
    _pendingParryCounterTarget = {};
    _parryCounterRequiredConsumeKeys.clear();
    _parryCounterGuardedConsumeKeys.clear();
}

Shared<BattleReactionController> BattleReactionController::Create()
{
    return make_shared<BattleReactionController>();
}

void BattleReactionController::Free()
{
    OnReactionWindowOpened.Clear();
    OnReactionWindowClosed.Clear();
    OnReactionSucceeded.Clear();
    OnCounterStarted.Clear();

    Clear_RuntimeState();
    Base::Free();
}
