#pragma once
#include "AnimNotifyState.h"

NS_BEGIN(Client)

class ANS_IdleOffset : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_IdleOffset)

public:
    ANS_IdleOffset() = default;
    ~ANS_IdleOffset() override = default;

public:
    string Get_TypeName() const override { return "ANS_IdleOffset"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    bool Try_GetPelvisWorldPosition(const AnimNotifyContext& context, Vec3& outWorldPos);

private:
    Vec3 _prevPelvisWorld{};
    bool _initialized = false;
};

NS_END
