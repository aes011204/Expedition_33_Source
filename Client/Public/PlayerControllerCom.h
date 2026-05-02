#pragma once

#include "Component.h"

NS_BEGIN(Client)

class PlayerControllerCom : public Component
{
public:
    PlayerControllerCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    PlayerControllerCom(const PlayerControllerCom& Prototype);
    virtual ~PlayerControllerCom();

public:


private:


private:



public:
    static Shared<PlayerControllerCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
    
};

NS_END
