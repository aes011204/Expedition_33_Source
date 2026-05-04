#pragma once

#include "Editor_Window.h"

NS_BEGIN(MainEditor)

class Scene_View final : public Editor_Window
{
public:
    /* 에디터 카메라 렌더링과 기즈모 편집을 붙일 씬 뷰포트 창을 생성한다. */
    Scene_View();
    virtual ~Scene_View() = default;

public:
    virtual void Update(float timeDelta) override;
    virtual void Render() override;

private:
    void Handle_GizmoShortcut();
    void Render_GizmoOverlay();

    void Render_ViewportCanvas();
    void Handle_FocusRequest();
    void Render_SelectedObjectGizmo();

private:
    /* 마지막으로 표시된 씬 뷰포트 크기. */
    ImVec2 _viewportSize = {};
    ImVec2 _viewportMin = {};
    ImVec2 _viewportMax = {};

    ImGuizmo::OPERATION _gizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE _gizmoMode = ImGuizmo::LOCAL;

public:
    static Shared<Scene_View> Create();
};

NS_END

