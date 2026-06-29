#include "Item_Chroma.h"

#include "GameDataTable.h"
#include "ClientInstance.h"
#include "PlayerInventory.h"

IMPLEMENT_REFLECTION(Item_Chroma)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Item_Chroma";
    info.category = "Item";

    PROPERTY_INT("크로마", _amount);

    return true;
}

Item_Chroma::Item_Chroma(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Item(device, context)
{
}

Item_Chroma::Item_Chroma(const Item_Chroma& prototype)
    : Item(prototype)
{
}

void Item_Chroma::Apply_PickupReward()
{
    Item::Apply_PickupReward();

    const auto& inventory = CLIENT->Get_PlayerInventory();
    CHECK_NULL(inventory);

    // 크로마 증가. 크로마 확인 텍스트를 탭눌렀을 때 보이게 해줘야할거같다.
    inventory->Add_Chroma(_amount);
}

Shared<Item_Chroma> Item_Chroma::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Item_Chroma>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : Item_Chroma");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Item_Chroma::Clone(void* arg)
{
    auto clone = make_shared<Item_Chroma>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Item_Chroma");
        return nullptr;
    }

    return clone;
}

void Item_Chroma::Free()
{
    Item::Free();
}
