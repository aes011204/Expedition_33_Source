#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_SimonCriticalGlow : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_SimonCriticalGlow)

public:
    AN_SimonCriticalGlow() = default;
    virtual ~AN_SimonCriticalGlow() = default;

public:
    string Get_TypeName() const override { return "AN_SimonCriticalGlow"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    bool _critical = true;
    float _duration = 1.f;
};

NS_END
