#pragma once

#include "Base.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

// Dodge, Parry, QTE 전투 리액션을 컨트롤할 클래스. BattleController에서 분리ㄴ
class BattleReactionController : public Base
{
public:
    struct BattleReactionInputResult
    {
        bool hasInput = false;  // 이번 프레임에 키 입력이 있었는지
        bool success = false;   // 성공 여부

        BattleReactionType type = BattleReactionType::None;
    };

public:
    BattleReactionController() = default;
    ~BattleReactionController() override = default;

public:
    void OpenWindow(BattleReactionType type);
    void CloseWindow(BattleReactionType type);

    void Clear();
    void Clear_RuntimeState(); // 전투 종료/초기화

    bool TryInput(BattleReactionType type);
    bool TryConsume(BattleReactionType& outResult);

    bool IsWindowOpen(BattleReactionType type) const;
    bool IsConsumed() const { return _isConsumed; }
    BattleReactionType Get_Result() const { return _result; }

    BattleReactionInputResult UpdateInput(const BattleChar& target);
    void UpdateAnimation();

private:
    bool CanReceiveInput(const BattleChar& target) const;
    bool IsHitOrDeadBattleAnim(BattleAnimType animType) const;
    void PlayReactionAnimation(const BattleChar& target, BattleReactionType reactionType, bool isSuccess);

private:
    bool _isDodgeOpen = false; 
    bool _isParryOpen = false; 

    bool _isConsumed = false; // 몬스터 애니메이션의 AN_Attack에서 아직 소비되지 않은 상태
    BattleReactionType _result = BattleReactionType::None;

    // Reaction(패링, 회피, QTE?)가 끝나면 Idle로 복귀시킬 대상
    BattleChar _reactionAnimTarget = {};

public:
    static Shared<BattleReactionController> Create();
    void Free() override;

};

NS_END
