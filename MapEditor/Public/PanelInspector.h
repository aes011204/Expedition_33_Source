#pragma once
#include "MEWindow.h"

NS_BEGIN(MapEditor)

struct SceneContext;

class PanelInspector final : public MEWindow
{
private:
    static constexpr uint64 _shapeNum =  magic_enum::enum_count<EZoneShape>();
    static constexpr uint64 _lightNum = magic_enum::enum_count<LightType>() - 1;

public:
    explicit PanelInspector(SceneContext* ctx);
    virtual ~PanelInspector() = default;

public:
    virtual void Render() override;

private:
    SceneContext* _ctx = {};
    const char* _shapes[_shapeNum] = {};
    const char* _lights[_lightNum] = {};

public:
    static Shared<PanelInspector> Create(SceneContext* ctx);
};

NS_END
