#pragma once

#include "Component.h"

NS_BEGIN(Client)

class CombatStatCom : public Component
{
public:
    struct CombatStatDesc
    {
        float maxHp = 100.f;
        float hp = 100.f;
        float attack = 10.f;
        float defense = 0.f;
    };

public:
    CombatStatCom(const ComPtr<Device>& device, const ComPtr<Context>& contexts);
    CombatStatCom(const CombatStatCom& Prototype);
    virtual ~CombatStatCom();

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;

public:
    

private:


private:
    float _maxHp = 100.f;
    float _hp = 100.f;
    float _attack = 10.f;
    float _defense = 0.f;

public:
    static Shared<CombatStatCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
    
};

NS_END
