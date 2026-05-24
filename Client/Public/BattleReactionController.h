#pragma once

#include "Base.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

// Dodge, Parry, QTE 전투 리액션을 컨트롤할 클래스. BattleController에서 분리
class BattleReactionController : public Base
{
public:
    BattleReactionController() = default;
    ~BattleReactionController() override = default;

public:
    void OpenWindow(BattleReactionType type);
    void CloseWindow(BattleReactionType type);
    void Clear();

    bool TryInput(BattleReactionType type);
    bool TryConsume(BattleReactionType& outResult);

    bool IsWindowOpen(BattleReactionType type) const;
    bool IsConsumed() const { return _isConsumed; }
    BattleReactionType Get_Result() const { return _result; }

private:
    bool _isDodgeOpen = false; 
    bool _isParryOpen = false; 

    bool _isConsumed = false; // 몬스터 애니메이션의 AN_Attack에서 아직 소비되지 않은 상태
    BattleReactionType _result = BattleReactionType::None;

public:
    static Shared<BattleReactionController> Create();
    void Free() override;

};

NS_END
