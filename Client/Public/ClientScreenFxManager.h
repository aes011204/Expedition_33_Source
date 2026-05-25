#pragma once
#include "Base.h"

NS_BEGIN(Client)

class ClientScreenFxManager final : public Base
{
public:
    void Play_BattleInCounterFx();
private:

public:
    static Shared<ClientScreenFxManager> Create();
};

NS_END
