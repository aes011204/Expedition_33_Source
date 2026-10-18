#pragma once

#include "Client_Defines.h"
#include "Level.h"

NS_BEGIN(Client)

class Level_ReplayTransition : public Level
{
public:
    Level_ReplayTransition(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~Level_ReplayTransition() override = default;

public:
    HRESULT Initialize() override;
    HRESULT Render() override;

public:
    static bool Is_SupportedReplayLevel(LevelType levelType);
    static Shared<Level> Create_ReplayLevel(LevelType levelType, const ComPtr<Device>& device, const ComPtr<Context>& context);
    static Shared<Level> Create_LoadingFallbackLevel(LevelType targetLevel, const ComPtr<Device>& device, const ComPtr<Context>& context);

public:
    static Shared<Level_ReplayTransition> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Free() override;
};

NS_END
