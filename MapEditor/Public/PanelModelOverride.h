#pragma once
#include "MEWindow.h"

NS_BEGIN(MapEditor)

struct SceneContext;

class PanelModelOverride final : public MEWindow
{
public:
    explicit PanelModelOverride(SceneContext* ctx);
    virtual ~PanelModelOverride() = default;

public:
    virtual void Render() override;

private:
    SceneContext* _ctx = {};

public:
    static Shared<PanelModelOverride> Create(SceneContext* ctx);
};

NS_END
