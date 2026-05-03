#include "Bounding.h"

Bounding::Bounding(ComPtr<Device> device, ComPtr<Context> context)
    : _device(device), _context(context)
{
}

HRESULT Bounding::Initialize(const FBoundingDesc& desc)
{


    return S_OK;
}

void Bounding::Free()
{
    Base::Free();
}
