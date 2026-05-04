#pragma once
#include "MEWindow.h"

NS_BEGIN(Engine)
class RenderTarget;
NS_END

NS_BEGIN(MapEditor)

class MECamera;
struct SceneContext;

class PanelViewport final : public MEWindow
{
public:
    PanelViewport(Shared<MECamera> camera, Shared<RenderTarget> rt, SceneContext* ctx);
    virtual ~PanelViewport() = default;

public:
    virtual void Update(float timeDelta) override;
    virtual void Render() override;

private:
    Shared<MECamera> _camera;
    Shared<RenderTarget> _rt;
    SceneContext* _ctx = {};

    bool _isHovered = false;

    ImGuizmo::OPERATION _gizmoOp = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE _gizmoMode = ImGuizmo::WORLD;

public:
    static Shared<PanelViewport> Create(Shared<MECamera> camera, Shared<RenderTarget> rt, SceneContext* ctx);
};

NS_END
