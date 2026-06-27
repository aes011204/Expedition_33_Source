#include "Level_ReplayTransition.h"

#include "Level_GamePlay.h"
#include "Level_GestralVillage.h"
#include "Level_Loading.h"
#include "Level_MapTest.h"
#include "Level_Simon.h"

Level_ReplayTransition::Level_ReplayTransition(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level(device, context)
{
}

HRESULT Level_ReplayTransition::Initialize()
{
    return S_OK;
}

HRESULT Level_ReplayTransition::Render()
{
    return S_OK;
}

bool Level_ReplayTransition::Is_SupportedReplayLevel(LevelType levelType)
{
    return levelType == LevelType::GamePlay ||
        levelType == LevelType::MapTest ||
        levelType == LevelType::Stage01 ||
        levelType == LevelType::GestralVillage ||
        levelType == LevelType::Simon;
}

Shared<Level> Level_ReplayTransition::Create_ReplayLevel(LevelType levelType, const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    switch (levelType)
    {
    case LevelType::GamePlay:
        return Level_GamePlay::Create(device.Get(), context.Get());
    case LevelType::MapTest:
        return Level_MapTest::Create(device.Get(), context.Get());
    case LevelType::GestralVillage:
        return Level_GestralVillage::Create(device.Get(), context.Get());
    case LevelType::Simon:
        return Level_Simon::Create(device.Get(), context.Get());

    default:
        return nullptr;
    }
}

Shared<Level> Level_ReplayTransition::Create_LoadingFallbackLevel(
    LevelType targetLevel,
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    return Level_Loading::Create(device.Get(), context.Get(), targetLevel);
}

Shared<Level_ReplayTransition> Level_ReplayTransition::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Level_ReplayTransition>(device, context);

    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Created : Level_ReplayTransition");
        return nullptr;
    }

    return instance;
}

void Level_ReplayTransition::Free()
{
    Level::Free();
}
