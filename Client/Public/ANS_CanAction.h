#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class ANS_CanAction final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_CanAction)

public:
    ANS_CanAction() = default;
    ~ANS_CanAction() override = default;

public:
    string Get_TypeName() const override { return "ANS_CanAction"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    void Set_CanAction(const AnimNotifyContext& context, bool enabled) const;

private:
    bool _canJump = true;   // 이 구간에서 JumpCounter 점프 캔슬 허용
    bool _canDodge = true;  // 이 구간에서 Dodge 캔슬 허용
    bool _canParry = true; // 이 구간에서 Parry 캔슬 허용
};

NS_END
