#pragma once
#include "Base.h"

NS_BEGIN(Client)
struct BattleChar;

class DamageSystem :
    public Base
{
public:

    DamageSystem();
    virtual ~DamageSystem();
public:

static void ApplyDamage(float damage,const BattleChar& target);
static void Heal(float amount, const BattleChar& target);

 static Shared<DamageSystem> Create();

 virtual void Free();
};


NS_END
