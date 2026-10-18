#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_StartCounter : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_StartCounter)

public:
    string Get_TypeName() const override { return "AN_StartCounter"; }
    void Execute(const AnimNotifyContext& context) override;

private:

};

NS_END
