#pragma once

#include <array>
#include "Base.h"
#include "Battle_Enum.h"
#include "Delegate.h"

NS_BEGIN(Client)

class BattleAction;// Dodge, Parry, QTE 전투 리액션을 컨트롤할 클래스. BattleController에서 분리ㄴ

class BattleReactionController : public Base
{
public:
    DECLARE_DELEGATE(
        FOnReactionWindowOpened,
        BattleReactionType /* ANS_Dodge, Parry로 열린 입력창. 튜토리얼 컨트롤러가 Dodge/Parry UI 띄우기 판단용*/
    );

    DECLARE_DELEGATE(
        FOnReactionWindowClosed,
        BattleReactionType /* 실패/만료/성공으로 입력창이 닫힌 Reaction 종류 */
    );

    // Reaction 성공 (Dodge, Parry)
    DECLARE_DELEGATE(
        FOnReactionSucceeded,
        BattleReactionType, /* 성공한 Reaction 종류 */
        const vector<BattleChar>& /* 성공 UI 위치 계산에 사용할 플레이어 */
    );

    DECLARE_DELEGATE(
        FOnCounterStarted,
        const vector<BattleChar>& /* Counter 애니메이션이 실제 시작된 플레이어들 */
    );

private:
    enum class ReactionAnimPhase
    {
        None,
        Reaction,   // Dodge Or Parry
        Counter,    // Parry 성공 이후 카운터로
        CounterReturn,
        END
    };

    enum class JumpCounterState
    {
        None,
        JumpSucceeded,
        CounterWindowOpen,
        CounterSucceeded,
        Started
    };

public:
    struct BattleReactionInputResult
    {
        bool hasInput = false;  // 이번 프레임에 키 입력이 있었는지
        bool success = false;   // 성공 여부

        BattleReactionType type = BattleReactionType::None;

        BattleChar apRewardTarget{};
        uint32 apRewardAmount = 0;
        EAPChangeReason apRewardReason = EAPChangeReason::Invalid;
    };

    struct CounterReturnRuntime
    {
        BattleChar target = {};
        Vec3 startPos = {};
        Vec3 goalPos = {};
        Vec3 goalRot = {};
        float prevRootRatio = 0.f;
    };

    struct ReactionInputContext
    {
        BattleReactionType type = BattleReactionType::None;
        uint32 stepIndex = UINT32_MAX;
        uint32 consumeKey = UINT32_MAX;
        vector<BattleChar> targets = {};
        BattleChar counterTarget = {};
        bool allowCounterOnSuccess = false;
        bool isEcho = false;
    };

    struct ActiveReactionWindowSnapshot
    {
        BattleReactionType type = BattleReactionType::None;
        float elapsedSec = 0.f;
        float remainingSec = 0.f;
        ReactionInputContext context = {};
    };

private:
    struct DelayedEchoReactionWindow
    {
        float delaySec = 0.f;
        float durationSec = 0.f;
        ReactionInputContext context = {};
        bool opened = false;
        bool inputConsumed = false;
    };

    struct ConsumedReactionInput
    {
        BattleReactionType type = BattleReactionType::None;
        uint32 consumeKey = UINT32_MAX;
    };

public:
    BattleReactionController() = default;
    ~BattleReactionController() override = default;

public:
    void OpenWindow(BattleReactionType type);
    void OpenWindow(BattleReactionType type, const BattleAction& turnAction, float durationSec);
    void CloseWindow(BattleReactionType type);

    void Clear();
    void Clear_RuntimeState(); // 전투 종료/초기화

    bool TryInput(BattleReactionType type, uint32 consumeKey);
    bool TryConsume(BattleReactionType& outResult, uint32 consumeKey);
    bool TryConsumeGuard(const BattleChar& target, uint32 stepIndex, uint32 consumeKey = UINT32_MAX);

    bool IsWindowOpen(BattleReactionType type) const;
    bool IsConsumed() const { return !_consumedReactionInputs.empty(); }
    BattleReactionType Get_Result() const { return _result; }

    BattleReactionInputResult SubmitInput(BattleReactionType type, const BattleAction& turnAction);
    BattleReactionInputResult SubmitInput(const ReactionInputContext& context);
    BattleReactionInputResult UpdateInput(const BattleAction& turnAction, bool autoParryEnabled = false);

    //BattleReactionInputResult UpdateTargetInput(const BattleChar& reactionTarget, const BattleChar& counterTarget, uint32 curAnimIndex, bool pressedQ, bool pressedE);
    void UpdateAnimation(float timeDelta);
    void Notify_TargetDamaged(const BattleChar& target);

    void StartCounterReturn();
    // void FinishCounterReturn();
    void UpdateCounterReturn(float timeDelta);
    void StartCounterReturnAll(ModelCom* sourceModel);

    void Queue_EchoReactionWindow(float delaySec, float durationSec, const ReactionInputContext& context);
    vector<ActiveReactionWindowSnapshot> Get_ActiveReactionWindowSnapshots() const;
    void Update_EchoReactionWindows(float timeDelta);
    void Clear_EchoReactionWindows();
    void Suppress_CounterForSourceStep(uint32 stepIndex);
    static uint32 Make_ReactionConsumeKey(uint32 sourceStepIndex, uint32 echoLaneIndex = 0);

public:
    void Set_CurSkillData(const Skill_Data* curSkillData)
    {
        _curSkillData = curSkillData;
        ClearConsumedInput();
        _echoCounterSuppressedSteps.clear();
        Clear_ParryCounterRuntime();
    };

    void StartCounter(ModelCom* sourceModel);
    void ApplyCounterDamage(ModelCom* sourceModel);
    void NotifyJumpCounterWindowBegin();
    void NotifyJumpCounterWindowEnd();
    bool TryStartQueuedJumpCounter(ModelCom* sourceModel);
    bool Try_GetJumpCounterTarget(BattleChar& outTarget) const;

    void BeginCounterTimeSection(ModelCom* sourceModel, float worldTimeScale, float counterFrameSpeed);
    void EndCounterTimeSection(ModelCom* sourceModel);
    bool IsBusy() const { return _animPhase != ReactionAnimPhase::None && !_reactionAnimTargets.empty(); }

    bool Is_GroupParryActive() const;

private:
    void ResetJumpCounterSequence();
    void ClearConsumedInput();
    bool Is_ConsumeKeyPending(uint32 consumeKey) const;
    bool CanReceiveInput(const BattleChar& target) const;
    bool IsHitOrDeadBattleAnim(BattleAnimType animType) const;

    void PlayReactionAnimation(
        const BattleChar& target,
        BattleReactionType reactionType,
        bool isSuccess,
        const BattleChar& counterTarget,
        uint32 curAttackIndex,
        bool allowCounterOnSuccess);

    void FinishReactionAnimation();
    void TryRestoreTargetToSpawnPose(const BattleChar& target) const;

    bool CanApplyCounterTimeSection(ModelCom* sourceModel) const;
    void ResetCounterTimeSection();

private:
    ReactionInputContext BuildReactionInputContext(BattleReactionType type, const BattleAction& turnAction) const;
    const ReactionInputContext* Find_ActiveEchoReactionContext(BattleReactionType type) const;
    void Mark_EchoReactionInputConsumed(const ReactionInputContext& context);
    bool Is_LastSkillAttackStep(uint32 stepIndex) const;
    bool Is_CounterSuppressedForSourceStep(uint32 stepIndex) const;
    void Register_ParryCounterRequiredKey(uint32 stepIndex, uint32 consumeKey);
    void Mark_ParryCounterGuardedKey(uint32 stepIndex, uint32 consumeKey);
    bool Is_ParryCounterGuardChainComplete(uint32 stepIndex) const;
    void Try_ArmParryCounterAfterGuard(const BattleChar& target, uint32 stepIndex);
    void Clear_ParryCounterRuntime();

public:
    FOnReactionWindowOpened OnReactionWindowOpened;
    FOnReactionWindowClosed OnReactionWindowClosed;
    FOnReactionSucceeded    OnReactionSucceeded;
    FOnCounterStarted       OnCounterStarted;

private:
    array<uint32, ETOI(BattleReactionType::END)> _reactionWindowOpenCounts = {};
    array<float, ETOI(BattleReactionType::END)> _reactionWindowElapsedSec = {};
    array<float, ETOI(BattleReactionType::END)> _reactionWindowRemainingSec = {};
    array<ReactionInputContext, ETOI(BattleReactionType::END)> _reactionWindowContexts = {};
    vector<DelayedEchoReactionWindow> _delayedEchoReactionWindows = {};
    uset<uint32> _echoCounterSuppressedSteps = {};

    vector<ConsumedReactionInput> _consumedReactionInputs = {}; // AN_Attack/echo damage에서 아직 소비되지 않은 입력들
    BattleReactionType _result = BattleReactionType::None;

    vector<BattleChar> _reactionAnimTargets = {};  // Reaction 애니메이션 재생 플레이어
    vector<CounterReturnRuntime> _counterReturnRuntimes = {};
    BattleChar _counterDamageTarget = {}; // Parry 성공 후 Counter 데미지를 받을놈
    BattleChar _pendingParryCounterTarget = {};
    umap<uint32, uset<uint32>> _parryCounterRequiredConsumeKeys = {};
    umap<uint32, uset<uint32>> _parryCounterGuardedConsumeKeys = {};

    ReactionAnimPhase _animPhase = ReactionAnimPhase::None;
    bool _isCounterPending = false;
    bool _isCounterDamageApplied = false;
    float _reactionAnimElapsed = 0.f;

    const Skill_Data* _curSkillData = {};
    JumpCounterState _jumpCounterState = JumpCounterState::None;
    uint32 _jumpCounterStepIndex = UINT32_MAX;
    BattleChar _jumpCounterTarget = {};
    bool _jumpCounterStarted = false;

    // 카운터
    bool _isCounterTimeSectionActive = false;
    float _counterTimeSectionRestoreScale = 1.f;
    Shared<ModelCom> _counterTimeSectionModel = nullptr;

public:
    static Shared<BattleReactionController> Create();
    void Free() override;

};

NS_END
