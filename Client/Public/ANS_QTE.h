#pragma once

#include "AnimNotifyState.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class ANS_QTE : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_QTE)

public:
    ANS_QTE() = default;
    ~ANS_QTE() override = default;

public:
    string Get_TypeName() const override { return "ANS_QTE"; }

    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    BattleQTERequest Make_Request() const;
    Vec2             Resolve_ScreenPosition(BattleQTEPreset preset) const;
    float            Resolve_ScreenScale(BattleQTEPreset preset) const;


private:
    // QTE 애니메이션에서 세팅된 값
    float _introDuration = 21.f / 63.f;
    float _progressDuration = 80.f / 120.f;
    float _perfectStartRatio = 61.f / 80.f;
    float _perfectEndRatio = 1.f;

    BattleQTEPreset _qtePreset = BattleQTEPreset::Center;
};

NS_END
