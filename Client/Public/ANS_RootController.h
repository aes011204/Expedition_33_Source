#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class ANS_RootController final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_RootController)

public:
    ANS_RootController() = default;
    ~ANS_RootController() override = default;

public:
    string Get_TypeName() const override { return "ANS_RootController"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    void Apply_Control(const AnimNotifyContext& context) const;

private:
    string _boneNames = "root,pelvis";
    Vec3 _translationScale = Vec3(1.f, 0.3f, 1.f);
    float _blendInSec = 0.f;
    float _blendOutSec = 0.f;
};

NS_END
