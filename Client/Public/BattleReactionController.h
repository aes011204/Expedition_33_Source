#pragma once

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

public:
    BattleReactionController() = default;
    ~BattleReactionController() override = default;

public:
    void OpenWindow(BattleReactionType type);
    void CloseWindow(BattleReactionType type);

    void Clear();
    void Clear_RuntimeState(); // 전투 종료/초기화

    bool TryInput(BattleReactionType type, uint32 stepIndex);
    bool TryConsume(BattleReactionType& outResult, uint32 stepIndex);
    bool TryConsumeGuard(const BattleChar& target, uint32 stepIndex);

    bool IsWindowOpen(BattleReactionType type) const;
    bool IsConsumed() const { return _isConsumed; }
    BattleReactionType Get_Result() const { return _result; }

    BattleReactionInputResult SubmitInput(BattleReactionType type, const BattleAction& turnAction);
    BattleReactionInputResult UpdateInput(const BattleAction& turnAction);
    //BattleReactionInputResult UpdateTargetInput(const BattleChar& reactionTarget, const BattleChar& counterTarget, uint32 curAnimIndex, bool pressedQ, bool pressedE);
    void UpdateAnimation(float timeDelta);
    void Notify_TargetDamaged(const BattleChar& target);

    void StartCounterReturn();
    // void FinishCounterReturn();
    void UpdateCounterReturn(float timeDelta);
    void StartCounterReturnAll(ModelCom* sourceModel);

public:
    void Set_CurSkillData(const Skill_Data* curSkillData) { _curSkillData = curSkillData; };

    void StartCounter(ModelCom* sourceModel);
    void ApplyCounterDamage(ModelCom* sourceModel);

    void BeginCounterTimeSection(ModelCom* sourceModel, float worldTimeScale, float counterFrameSpeed);
    void EndCounterTimeSection(ModelCom* sourceModel);
    bool IsBusy() const { return _animPhase != ReactionAnimPhase::None && !_reactionAnimTargets.empty(); }

    bool Is_GroupParryActive() const;

private:
    bool CanReceiveInput(const BattleChar& target) const;
    bool IsHitOrDeadBattleAnim(BattleAnimType animType) const;
    void PlayReactionAnimation(
        const BattleChar& target,
        BattleReactionType reactionType,
        bool isSuccess,
        const BattleChar& counterTarget,
        uint32 curAttackIndex);

    void FinishReactionAnimation();
    void TryRestoreTargetToSpawnPose(const BattleChar& target) const;

    bool CanApplyCounterTimeSection(ModelCom* sourceModel) const;
    void ResetCounterTimeSection();

public:
    FOnReactionWindowOpened OnReactionWindowOpened;
    FOnReactionSucceeded    OnReactionSucceeded;
    FOnCounterStarted       OnCounterStarted;

private:
    bool _isDodgeOpen = false;
    bool _isParryOpen = false;
    bool _isJumpOpen = false;
    bool _isJumpCounterOpen = false;

    bool _isConsumed = false; // 몬스터 애니메이션의 AN_Attack에서 아직 소비되지 않은 상태
    BattleReactionType _result = BattleReactionType::None;

    uint32 _consumedStepIndex = UINT32_MAX;

    vector<BattleChar> _reactionAnimTargets = {};  // Reaction 애니메이션 재생 플레이어
    vector<CounterReturnRuntime> _counterReturnRuntimes = {};
    BattleChar _counterDamageTarget = {}; // Parry 성공 후 Counter 데미지를 받을놈

    ReactionAnimPhase _animPhase = ReactionAnimPhase::None;
    bool _isCounterPending = false;
    bool _isCounterDamageApplied = false;
    float _reactionAnimElapsed = 0.f;

    const Skill_Data* _curSkillData = {};

    // 카운터
    bool _isCounterTimeSectionActive = false;
    float _counterTimeSectionRestoreScale = 1.f;
    Shared<ModelCom> _counterTimeSectionModel = nullptr;

public:
    static Shared<BattleReactionController> Create();
    void Free() override;

};

NS_END
