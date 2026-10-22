#pragma once

#include "Battle_Enum.h"

NS_BEGIN(Client)

class TurnLifecycle
{
public:
    void Begin(const BattleChar& actor);
    void Reset();
    bool TryCommitEnd();

    const BattleChar& Get_CurrentActor() const { return _currentActor; }

private:
    BattleChar _currentActor{};
    bool _endCommitted = true;
};

NS_END
