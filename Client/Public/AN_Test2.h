#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_Test2 : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_Test2)

public:
    AN_Test2() = default;
    virtual ~AN_Test2() = default;

public:
    string Get_TypeName() const override { return "AN_Test2"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    int     _testInt = 10;
    Vec3    _testVec3 = Vec3::Zero;
};

NS_END
