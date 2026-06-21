#pragma once

#include "AnimNotify.h"
#include "SL_Types.h"

NS_BEGIN(Client)

class AN_SL_State final : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_SL_State)

public:
    AN_SL_State() = default;
    ~AN_SL_State() override = default;

public:
    string Get_TypeName() const override { return "AN_SL_State"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    SL_PlayerState _nextState = SL_PlayerState::Idle;
};

NS_END
