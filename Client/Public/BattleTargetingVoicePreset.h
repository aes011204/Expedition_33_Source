#pragma once

#include "Base.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class BattleTargetingVoicePreset final : public Base
{
public:
    BattleTargetingVoicePreset();
    ~BattleTargetingVoicePreset() override = default;

public:
    HRESULT Initialize();

    bool Play(
        const ActionRequest& request,
        const vector<BattleChar>& targetedHeroes,
        const vector<BattleChar>& battleHeroes,
        const Skill_Data* selectedSkill);

    void Reset_ForTurn(const BattleChar& actor);

private:
    bool Try_PlaySingleTargetVoice(const BattleChar& target, const vector<BattleChar>& battleHeroes);
    bool Try_PlayAreaWarningVoice(const vector<BattleChar>& targetedHeroes) const;
    bool Try_PlaySelfTargetingVoice(const BattleChar& target) const;
    bool Try_PlayWarningVoice(const BattleChar& target, const vector<BattleChar>& battleHeroes) const;

private:
    Weak<Character_BattleBase> _currentTurnActor{};
    bool _playedThisTurn = false;

public:
    static Unique<BattleTargetingVoicePreset> Create();
    void Free() override;
};

NS_END
