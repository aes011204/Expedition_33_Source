#pragma once
#include "MEWindow.h"

NS_BEGIN(MapEditor)

struct SceneContext;

class PanelModelOverride final : public MEWindow
{
private:
    static constexpr int32 slotCount = static_cast<int32>(magic_enum::enum_count<MaterialTextureSlot>() - 1);

public:
    explicit PanelModelOverride(SceneContext* ctx);
    virtual ~PanelModelOverride() override = default;

public:
    virtual void Render() override;

private:
    SceneContext* _ctx = {};

public:
    static Shared<PanelModelOverride> Create(SceneContext* ctx);
};

NS_END
