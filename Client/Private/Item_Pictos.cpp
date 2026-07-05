#include "Item_Pictos.h"

#include "GameDataTable.h"
#include "ClientInstance.h"
#include "PlayerInventory.h"
#include "Unlock_Manager.h"

IMPLEMENT_REFLECTION(Item_Pictos)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Item_Pictos";
    info.category = "Item";

    PROPERTY_ENUM("픽토스 타입", _pictosType, Item_Pictos::PictosPickupType);
    PROPERTY_WSTRING("획득 이펙트", _pickupEffectName);

    return true;
}

Item_Pictos::Item_Pictos(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Item(device, context)
{
}

Item_Pictos::Item_Pictos(const Item_Pictos& prototype)
    : Item(prototype)
    , _pictosType(prototype._pictosType)
{
}

void Item_Pictos::Apply_PickupReward()
{
    const string pictosId = Resolve_PictosId();
    if (pictosId.empty())
        return;

    const GameDataTable::PictosDesc* pictosDesc = CLIENT->Get_PictosDesc(pictosId);
    CHECK_NULL(pictosDesc);

    if (pictosDesc->getItemUi.empty())
    {
        LOG_WARN("[ItemPickup] pictos id={} has empty get item ui", pictosId);
        return;
    }

    const auto& inventory = CLIENT->Get_PlayerInventory();
    CHECK_NULL(inventory);

    const bool alreadyUnlocked = inventory->Is_PictosUnlocked(pictosId);
    inventory->Unlock_Pictos(pictosId);

    if (!alreadyUnlocked)
    {
        if (Unlock_Manager* unlockManager = CLIENT->Get_UnlockManager())
            unlockManager->Queue_GetPictosPresentation(pictosId);
    }
}

string Item_Pictos::Resolve_PictosId() const
{
    return string(magic_enum::enum_name(_pictosType));
}

Shared<Item_Pictos> Item_Pictos::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Item_Pictos>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : Item_Pictos");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Item_Pictos::Clone(void* arg)
{
    auto clone = make_shared<Item_Pictos>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Item_Pictos");
        return nullptr;
    }

    return clone;
}

void Item_Pictos::Free()
{
    Item::Free();
}
