#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_ReturnBattleIdle : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_ReturnBattleIdle)

public:
    AN_ReturnBattleIdle() = default;
    ~AN_ReturnBattleIdle() override = default;

public:
    string Get_TypeName() const override { return "AN_ReturnBattleIdle"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    float _blendTime = 0.2f;
};

NS_END
