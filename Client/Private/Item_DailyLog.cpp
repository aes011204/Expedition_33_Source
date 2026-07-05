#include "Item_DailyLog.h"

#include "ClientInstance.h"

#include "GameInstance.h"

namespace
{
    JournalOpenAnimation Resolve_JournalOpenAnimation(LevelType levelType)
    {
        switch (levelType)
        {
        case LevelType::Stage01:
        case LevelType::GestralVillage:
            return JournalOpenAnimation::JournalMap;

        case LevelType::GamePlay:
        default:
            return JournalOpenAnimation::JournalUI;
        }
    }
}

IMPLEMENT_REFLECTION(Item_DailyLog)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Item_DailyLog";
    info.category = "Item";

    return true;
}

Item_DailyLog::Item_DailyLog(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Item(device, context)
{
}

Item_DailyLog::Item_DailyLog(const Item_DailyLog& prototype)
    : Item(prototype)
{
}

Shared<Item_DailyLog> Item_DailyLog::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Item_DailyLog>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : Item_DailyLog");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Item_DailyLog::Clone(void* arg)
{
    auto clone = make_shared<Item_DailyLog>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Item_DailyLog");
        return nullptr;
    }

    return clone;
}

void Item_DailyLog::Apply_PickupReward()
{
    CLIENT->Open_Journal(Resolve_JournalOpenAnimation(static_cast<LevelType>(GAME->Current_LevelIndex())));
}

void Item_DailyLog::Free()
{
    Item::Free();
}
