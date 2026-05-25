#include "EffectCom.h"



EffectCom::EffectCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component(device, context)
{
}

EffectCom::~EffectCom()
{
}

HRESULT EffectCom::Initialize_Prototype()
{
    return Component::Initialize_Prototype();
}

HRESULT EffectCom::Initialize(void* arg)
{
    return Component::Initialize(arg);
}

void EffectCom::BeginPlay()
{
    Component::BeginPlay();
}

void EffectCom::Update(float timeDelta)
{

}

Shared<EffectCom> EffectCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<EffectCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : EffectCom");
        return nullptr;
    }

    return instance;
}

Shared<Component> EffectCom::Clone(void* arg)
{
    auto clone = make_shared<EffectCom>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : EffectCom");
        return nullptr;
    }

    return clone;
}

void EffectCom::Free()
{
    Component::Free();
}
