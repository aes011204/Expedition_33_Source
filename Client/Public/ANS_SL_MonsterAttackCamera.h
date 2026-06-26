#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class SL_Base;

class ANS_SL_MonsterAttackCamera final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_SL_MonsterAttackCamera)

public:
    ANS_SL_MonsterAttackCamera() = default;
    ~ANS_SL_MonsterAttackCamera() override = default;

public:
    string Get_TypeName() const override { return "ANS_SL_MonsterAttackCamera"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    Shared<SL_Base> Resolve_Attacker(GameObject* notifyOwner) const;

private:
    string _presetKey = "MonsterAttack_SL_Dualliste_Dualliste_Duallist_Skill_05";
    bool _restoreOnEnd = true;
};

NS_END
