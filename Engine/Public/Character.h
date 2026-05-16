#pragma once

#include "ContainerObject.h"

NS_BEGIN(Engine)

class ModelCom;
class PartObject;

class ENGINE_DLL Character abstract : public ContainerObject
{
public:
    Character(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Character(const Character& Prototype);
    virtual ~Character();
    
public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;

public:
    void Free() override;

};

NS_END
