#pragma once
#include "MEWindow.h"

NS_BEGIN(Engine)
class RenderTarget;
NS_END

NS_BEGIN(MapEditor)

class MECamera;

class PanelViewport final : public MEWindow
{
public:
    PanelViewport();
    virtual ~PanelViewport() = default;

public:
    void SetCamera(Shared<MECamera> camera) { _camera = camera; }
    void SetRenderTarget(Shared<RenderTarget> rt) { _rt = rt; }
    void SetSelectedMatrix(Matrix* mat) { _selectedMatrix = mat; }

public:
    virtual void Update(float timeDelta) override;
    virtual void Render() override;

private:
    Shared<MECamera> _camera;
    Shared<RenderTarget> _rt;

    bool _isHovered = false;

    ImGuizmo::OPERATION _gizmoOp = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE _gizmoMode = ImGuizmo::WORLD;
    Matrix* _selectedMatrix = {};

public:
    static Shared<PanelViewport> Create();
};

NS_END
