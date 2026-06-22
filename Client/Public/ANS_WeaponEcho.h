#pragma once

#include "AnimNotifyState.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class Mon_Simon_Weapon;
class BattleAction;

class ANS_WeaponEcho : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_WeaponEcho)

public:
    ANS_WeaponEcho() = default;
    ~ANS_WeaponEcho() override = default;

public:
    string Get_TypeName() const override { return "ANS_WeaponEcho"; }

    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

    static void Queue_EchoAttackSteps(
        const AnimNotifyContext& context,
        const BattleChar& attacker,
        const vector<BattleChar>& targets,
        const BattleChar& reactionTarget,
        uint32 sourceStepIndex,
        AttackType attackType,
        float powerMultiplier);

    static void Queue_EchoReactionWindowsFromNotify(
        const AnimNotifyContext& context,
        BattleReactionType reactionType,
        const BattleAction& turnAction,
        float sourceWindowDurationSec);

    static void Clear_ActiveEchoWindows();

private:
    Mon_Simon_Weapon* Resolve_TargetSimonWeapon(const AnimNotifyContext& context) const;
    uint32  Resolve_EchoCount(const AnimNotifyContext& context) const;
    bool    Is_SecondEchoPhase(const AnimNotifyContext& context) const;
    void    Record_Sample(const AnimNotifyContext& context, bool force);
    float   Get_ElapsedSec(const AnimNotifyContext& context) const;

private:
    wstring _partTag = L"Part_Weapon";
    float   _sampleInterval = 0.03f;
    float   _delaySec = 0.2f;

    int32   _echoCountOverride = 0;

    float   _lastSampleElapsedSec = -1.f;
    float   _stateStartTimeSec = 0.f;
};

NS_END
