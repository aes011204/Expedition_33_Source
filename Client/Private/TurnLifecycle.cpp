#include "TurnLifecycle.h"

NS_BEGIN(Client)

void TurnLifecycle::Begin(const BattleChar& actor)
{
    _currentActor = actor;
    _endCommitted = false;
}

void TurnLifecycle::Reset()
{
    _currentActor = {};
    _endCommitted = true;
}

bool TurnLifecycle::TryCommitEnd()
{
    if (_endCommitted)
        return false;

    _endCommitted = true;
    return true;
}

NS_END
