#pragma once

#include "Item.h"

NS_BEGIN(Client)

class Item_Chroma : public Item
{
    GENERATED_GAMEOBJECT(Item_Chroma, ObjectType::Item)

public:
    Item_Chroma(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Item_Chroma(const Item_Chroma& prototype);
    ~Item_Chroma() override = default;

protected:
    const char* Get_ItemTypeName() const override { return "Chroma"; }
    void Apply_PickupReward() override;

public:
    static Shared<Item_Chroma> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
