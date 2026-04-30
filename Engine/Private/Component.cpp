#include "Component.h"
#include "GameInstance.h"

Component::Component(ComPtr<Device> device, ComPtr<Context> context)
    : _device{ device }
    , _context{ context }
{
}

Component::Component(const Component& Prototype)
    : _device{ Prototype._device }
    , _context{ Prototype._context }
{
}

HRESULT Component::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Component::Initialize(void* pArg)
{
    return S_OK;
}


void Component::Free()
{
    __super::Free();
}



