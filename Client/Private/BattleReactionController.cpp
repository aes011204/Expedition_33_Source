#include "BattleReactionController.h"

void BattleReactionController::OpenWindow(BattleReactionType type)
{
    if (type == BattleReactionType::Dodge)
        _isDodgeOpen = true;
    else if (type == BattleReactionType::Parry)
        _isParryOpen = true;

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
}

bool BattleReactionController::TryInput(BattleReactionType type)
{
    if (_isConsumed)
        return false;

    if (!IsWindowOpen(type))
        return false;

    _isConsumed = true;
    _result = type;

    return true;
}

bool BattleReactionController::TryConsume(BattleReactionType& outResult)
{
    if (!_isConsumed)
        return false;

    outResult = _result;
    Clear();

    return outResult == BattleReactionType::Dodge || outResult == BattleReactionType::Parry;
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

Shared<BattleReactionController> BattleReactionController::Create()
{
    return make_shared<BattleReactionController>();
}

void BattleReactionController::Free()
{
    Base::Free();
}
