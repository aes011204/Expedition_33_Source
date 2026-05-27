#pragma once
#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_EndTurn : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_EndTurn)

public:
    AN_EndTurn() = default;
    virtual ~AN_EndTurn() = default;

public:
    string Get_TypeName() const override { return "AN_EndTurn"; }
    void Execute(const AnimNotifyContext& context) override;
};

NS_END
