#pragma once
#include "Base.h"
NS_BEGIN(Client)
class UtilitySystem :
    public Base
{
public:
    UtilitySystem();
    ~UtilitySystem() override;

public:
    HRESULT Initialize() ;

    void Update(float timeDelta) ;
    void LateUpdate(float timeDelta) ;



public:
    void Free() override;

public:
    static Shared<UtilitySystem> Create();

};

NS_END
