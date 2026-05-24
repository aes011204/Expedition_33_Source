#pragma once
#include "AnimNotify.h"
NS_BEGIN(Client)
class AN_Change_Anim : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_Change_Anim)

public:
    AN_Change_Anim() = default;
    virtual ~AN_Change_Anim() = default;

public:
    string Get_TypeName() const override { return "AN_Change_Anim"; }
    void Execute(const AnimNotifyContext& context) override;

private:

    enum class BattleAnimType _animType = {};
};

NS_END

