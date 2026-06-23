#pragma once
#include "AnimNotifyState.h"

NS_BEGIN(Client)
class ANS_WorldTimeScale : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_WorldTimeScale)

public:
    string Get_TypeName() const override {return "ANS_WorldTimeScale";}

    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    float _worldTimeScale = 1.f;
    float _restoreWorldTimeScale = 1.f;
};
NS_END
