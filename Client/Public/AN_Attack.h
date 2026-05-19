#pragma once
#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_Attack : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_Attack)

public:
    AN_Attack() = default;
    virtual ~AN_Attack() = default;

public:
    string Get_TypeName() const override { return "AN_Attack"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    int     _testInt = 10;
    Vec3    _testVec3 = Vec3::Zero;
};

NS_END
