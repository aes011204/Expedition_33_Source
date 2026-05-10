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
    virtual void Pre_Render() override;
    virtual void Render() override;

private:
    void Handle_GizmoShortcut();
    void Render_GizmoOverlay();
    void Render_PreviewViewportOverlay();
    void Sync_PreviewViewportInput();

    /* MRT 디버그 렌더 */
    void Render_MRTDebugPopup();

    /* 렌더 타겟이 연결되기 전까지 사용할 임시 뷰포트 영역을 그린다. */
    void Render_ViewportCanvas();
    void Handle_FocusRequest();
    void Render_SelectedObjectGizmo();

private: /* Prefab Preview */ 
    void Set_DragPreviewObject(const string& prefabName);
    void Clear_DragPreviewObject();
    void Tick_DragPreview(float timeDelta);

private:
    /* 마지막으로 표시된 씬 뷰포트 크기. */
    ImVec2 _viewportSize = {};
    ImVec2 _viewportCanvasMin = {};
    ImVec2 _viewportCanvasMax = {};
    ImVec2 _viewportMin = {};
    ImVec2 _viewportMax = {};

    ImGuizmo::OPERATION _gizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE _gizmoMode = ImGuizmo::LOCAL;
    bool _altDuplicateActive = false;
    /* MRT 디버그 전체 드로우 영역 크기. */
    ImVec2 _mrtDebugDrawArea = ImVec2(0.f, 0.f);
    /* DrawArea 기본값을 씬 뷰 크기로 한 번만 동기화했는지 여부. */
    bool _mrtDebugInitialized = false;

    int _pendingPreviewViewportWidth = 0;
    int _pendingPreviewViewportHeight = 0;

private: /* Prefab Preview */
    Shared<GameObject>  _dragPreviewObject;
    string              _dragPreviewPrefabName{};
    POINT               _dragPreviewPixelPos{};

    bool                _dragPreviewRequestPending = false;
    bool                _dragPreviewHasWorldPos = false;
    Vec3                _dragPreviewWorldPos = Vec3::Zero;

public:
    static Shared<Scene_View> Create();
};

NS_END

