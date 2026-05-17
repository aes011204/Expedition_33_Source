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

HRESULT Character::Initialize_Prototype()
{
    return ContainerObject::Initialize_Prototype();
}

HRESULT Character::Initialize(void* arg)
{
    return ContainerObject::Initialize(arg);
}

void Character::BeginPlay()
{
    ContainerObject::BeginPlay();
}

void Character::Free()
{
    ContainerObject::Free();
}
