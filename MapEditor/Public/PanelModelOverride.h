#pragma once
#include "MEWindow.h"

NS_BEGIN(MapEditor)

struct SceneContext;
class PanelViewport;

class PanelModelOverride final : public MEWindow
{
private:
    static constexpr int32 slotCount = static_cast<int32>(magic_enum::enum_count<MaterialTextureSlot>() - 1);

public:
    explicit PanelModelOverride(SceneContext* ctx, Shared<PanelViewport> viewport = nullptr);
    virtual ~PanelModelOverride() override = default;

public:
    virtual void Render() override;

private:
    void RenderSelectedMaterialOverride();

private:
    SceneContext* _ctx = {};
    Shared<PanelViewport> _viewport = {};
    int32 _selectedMaterialIndex = 0;
    bool _liveApplyMaterialOverride = true;

public:
    static Shared<PanelModelOverride> Create(SceneContext* ctx, Shared<PanelViewport> viewport = nullptr);
};

NS_END
