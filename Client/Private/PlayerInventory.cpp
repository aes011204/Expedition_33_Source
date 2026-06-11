#include "PlayerInventory.h"

void PlayerInventory::Clear()
{
    _chroma = 0;
}

Shared<PlayerInventory> PlayerInventory::Create()
{
    return make_shared<PlayerInventory>();
}

void PlayerInventory::Free()
{
    Clear();

    Base::Free();
}
