#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_VanishDissolve : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_VanishDissolve)

public:
    enum class DissolveType
    {
        Switch, Vanish, END
    };

public:
    string Get_TypeName() const override { return "AN_VanishDissolve"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    DissolveType _type = DissolveType::END;
    float _duration = 0.7f;

};

NS_END
