#pragma once

#include "Base.h"

namespace Engine
{
    class Character;
}

NS_BEGIN(Client)

    class BuffInstance :
    public Base
{
public:
    int duration;
    int stack;

    virtual void OnTurnStart(Shared<Character> owner) {}
    virtual void OnTurnEnd(Shared<Character> owner) {}

    virtual void OnReceiveDamage(DamageEvent& evt) {}
};

NS_END
