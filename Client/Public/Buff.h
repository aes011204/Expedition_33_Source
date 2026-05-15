#pragma once
#include "Base.h"
NS_BEGIN(Client)
class Buff :
    public Base
{
public:
    Buff();
    virtual ~Buff();
public:
    virtual void OnTurnStart(){};
    virtual void OnTurnEnd(){};

    virtual void OnModifyDamage(DamageContext ctx){};

    uint32 ReduceStack()
    {
        uint32 tmpstack = _stack - 1;

        if(tmpstack <= 0 )
        {
            _stack = 0;
            //_pendingDead = true; // 이거 시스템에서 처리
        }
        else
        {
            return tmpstack;
        }
    };

    bool Is_MarkDestroy() { return _markDestroy;}
private:

    int _duration={};
    uint32 _stack = 1;

    bool _markDestroy {false};

};

NS_END
