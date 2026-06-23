#include "SL_Base.h"

SL_Base::SL_Base(ComPtr<Device> device, ComPtr<Context> context)
    : Character_WorldBase(device, context)
{
}

SL_Base::SL_Base(const SL_Base& prototype)
    : Character_WorldBase(prototype)
{
}
