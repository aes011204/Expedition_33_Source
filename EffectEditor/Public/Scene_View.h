#pragma once

#include "Editor_Window.h"

NS_BEGIN(EffectEditor)

class Scene_View final : public Editor_Window
{
public:
    Scene_View();
    ~Scene_View() override = default;

public:
    void Update(float timeDelta) override;
    void Render() override;

private:
    void Handle_GizmoShortcut();
    void Render_GizmoOverlay();
    void Render_PreviewViewportOverlay();
    void Sync_PreviewViewportInput();

    void Render_ViewportCanvas();
    void Handle_FocusRequest();
    void Render_SelectedObjectGizmo();

private:
    ImVec2 _viewportSize = {};
    ImVec2 _viewportMin = {};
    ImVec2 _viewportMax = {};

    ImGuizmo::OPERATION _gizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE _gizmoMode = ImGuizmo::LOCAL;

    int _pendingPreviewViewportWidth = 0;
    int _pendingPreviewViewportHeight = 0;

public:
    static Shared<Scene_View> Create();
};

NS_END
