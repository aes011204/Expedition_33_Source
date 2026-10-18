#pragma once

#include "Base.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class BattleSoundPreset final : public Base
{
public:
    BattleSoundPreset();
    ~BattleSoundPreset() override = default;

public:
    HRESULT Initialize();

    bool Play_MonsterTargetingVoice(
        const ActionRequest& request,
        const vector<BattleChar>& targetedHeroes,
        const vector<BattleChar>& battleHeroes,
        const Skill_Data* selectedSkill);
    bool Play_TurnStartVoice(const BattleChar& focusedActor);
    bool Play_ItemUseVoice(const ActionRequest& request) const;
    bool Play_ChromaPickupItemSound() const;
    bool Play_PictosPickupItemSound() const;

    void Reset_ForTurn(const BattleChar& actor);

private:
    bool Play_PickupItemSound(const wchar_t* itemSfxCueId) const;
    bool Try_PlaySingleTargetVoice(const BattleChar& target, const vector<BattleChar>& battleHeroes);
    bool Try_PlayAreaWarningVoice(const vector<BattleChar>& targetedHeroes) const;
    bool Try_PlaySelfTargetingVoice(const BattleChar& target) const;
    bool Try_PlayWarningVoice(const BattleChar& target, const vector<BattleChar>& battleHeroes) const;

private:
    Weak<Character_BattleBase> _monsterTargetingTurnActor{};
    bool _monsterTargetingPlayedThisTurn = false;

    Weak<Character_BattleBase> _turnStartActor{};
    bool _turnStartPlayedThisTurn = false;

public:
    static Unique<BattleSoundPreset> Create();
    void Free() override;
};

NS_END
