#pragma once
#include "MEWindow.h"

NS_BEGIN(MapEditor)

struct SceneContext;

class PanelInspector final : public MEWindow
{
public:
    explicit PanelInspector(SceneContext* ctx);
    virtual ~PanelInspector() = default;

public:
    virtual void Render() override;

private:
    SceneContext* _ctx = {};

public:
    static Shared<PanelInspector> Create(SceneContext* ctx);
};

NS_END
