#pragma once
#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_Battle_EndAnim :public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_Battle_EndAnim)

public:
    AN_Battle_EndAnim() = default;
    virtual ~AN_Battle_EndAnim() = default;

public:
    string Get_TypeName() const override { return "AN_Battle_EndAnim"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    string _nxtAnimName = {};
};

NS_END

