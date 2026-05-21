#pragma once
#include "AnimNotify.h"
NS_BEGIN(Client)
class AN_EnterAnim : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_EnterAnim)

public:
    AN_EnterAnim() = default;
    virtual ~AN_EnterAnim() = default;

public:
    string Get_TypeName() const override { return "AN_EnterAnim"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    string _nxtAnimName = {};
};

NS_END

