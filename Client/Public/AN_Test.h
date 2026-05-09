#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_Test : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_Test)

public:
    AN_Test() = default;
    virtual ~AN_Test() = default;

public:
    string Get_TypeName() const override { return "AN_Test"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    int     _testInt = 10;
    Vec3    _testVec3 = Vec3::Zero;
};

NS_END
