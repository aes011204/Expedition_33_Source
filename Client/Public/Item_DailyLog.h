#pragma once

#include "Item.h"

NS_BEGIN(Client)

class Item_DailyLog : public Item
{
    GENERATED_GAMEOBJECT(Item_DailyLog, ObjectType::Item)

public:
    Item_DailyLog(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Item_DailyLog(const Item_DailyLog& prototype);
    ~Item_DailyLog() override = default;

protected:
    const char* Get_ItemTypeName() const override { return "DailyLog"; }

public:
    static Shared<Item_DailyLog> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
