#pragma once

#include "ContainerObject.h"

NS_BEGIN(Engine)

class ENGINE_DLL Character : public ContainerObject
{
public:
    Character(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Character(const Character& Prototype);
    virtual ~Character();
    
public:
    

};

NS_END
