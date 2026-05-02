#include "Component.h"
#include "GameInstance.h"

Component::Component(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device{ device }
    , _context{ context }
{
}

Component::Component(const Component& prototype)
    : _device{ prototype._device }
    , _context{ prototype._context }
{
}

HRESULT Component::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Component::Initialize(void* arg)
{
    return S_OK;
}


void Component::Free()
{
    __super::Free();
}



