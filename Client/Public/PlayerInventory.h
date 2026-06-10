#pragma once

#include "Base.h"

NS_BEGIN(Client)

class PlayerInventory final : public Base
{
public:
    PlayerInventory() = default;
    ~PlayerInventory() override = default;

public:
    uint32  Get_Chroma() const { return _chroma; }
    void    Add_Chroma(uint32 amount) { _chroma += amount; }
    void    Clear();

private:
    uint32 _chroma = 0;

    // TODO : Weapon, Item

public:
    static Shared<PlayerInventory> Create();
    void Free() override;
};

NS_END
