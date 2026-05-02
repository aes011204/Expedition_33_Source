#include "Character.h"

Character::Character(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : ContainerObject(device, context)
{
}

Character::Character(const Character& Prototype)
    : ContainerObject(Prototype)
{
}

Character::~Character()
{
}
