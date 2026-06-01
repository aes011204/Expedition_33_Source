#pragma once

#include "Base.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class BattleAction;// Dodge, Parry, QTE 전투 리액션을 컨트롤할 클래스. BattleController에서 분리ㄴ

class BattleReactionController : public Base
{
private:
    enum class ReactionAnimPhase
    {
        None,
        Reaction,   // Dodge Or Parry
        Counter,    // Parry 성공 이후 카운터로
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

    bool IsWindowOpen(BattleReactionType type) const;
    bool IsConsumed() const { return _isConsumed; }
    BattleReactionType Get_Result() const { return _result; }

    BattleReactionInputResult UpdateInput(const BattleAction& turnAction);
    BattleReactionInputResult UpdateTargetInput(const BattleChar& reactionTarget, const BattleChar& counterTarget, uint32 curAnimIndex, bool pressedQ, bool pressedE);
    void UpdateAnimation();

public:
    void StartCounter(ModelCom* sourceModel, const BattleSlowDesc& slowDesc, float counterPlayRate);
    void ApplyCounterDamage(ModelCom* sourceModel);

    void Set_CurSkillData(const Skill_Data* curSkillData) { _curSkillData = curSkillData; };
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


private:
    bool _isDodgeOpen = false; 
    bool _isParryOpen = false; 

    bool _isConsumed = false; // 몬스터 애니메이션의 AN_Attack에서 아직 소비되지 않은 상태
    BattleReactionType _result = BattleReactionType::None;

    uint32 _consumedStepIndex = UINT32_MAX;

    vector<BattleChar> _reactionAnimTargets = {};  // Reaction 애니메이션 재생 플레이어
    BattleChar _counterDamageTarget = {}; // Parry 성공 후 Counter 데미지를 받을놈

    ReactionAnimPhase _animPhase = ReactionAnimPhase::None;
    bool _isCounterPending = false;
    bool _isCounterDamageApplied = false;

    const Skill_Data* _curSkillData = {};

public:
    static Shared<BattleReactionController> Create();
    void Free() override;

};

NS_END
