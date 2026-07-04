#pragma once

#include "Base.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class BattleTurnStartVoicePreset final : public Base
{
public:
    BattleTurnStartVoicePreset();
    ~BattleTurnStartVoicePreset() override = default;

public:
    HRESULT Initialize();

    bool Play(const BattleChar& focusedActor);
    void Reset_ForTurn(const BattleChar& actor);

private:
    Weak<Character_BattleBase> _currentTurnActor{};
    bool _playedThisTurn = false;

public:
    static Unique<BattleTurnStartVoicePreset> Create();
    void Free() override;
};

NS_END
