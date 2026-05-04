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
    virtual void Update(float fTimeDelta) override;
    virtual void Render() override;

private:
    /* 트랜스폼 기즈모 모드를 바꾸는 간단한 툴바를 그린다. */
    void Render_GizmoToolbar();

    /* MRT 디버그 렌더 */
    void Render_MRTDebugPopup();

    /* 렌더 타겟이 연결되기 전까지 사용할 임시 뷰포트 영역을 그린다. */
    void Render_ViewportCanvas();

private:
    /* 마지막으로 표시된 씬 뷰포트 크기. */
    ImVec2 _viewportSize = {};
    /* 씬 기즈모의 현재 트랜스폼 동작. */
    ImGuizmo::OPERATION _gizmoOperation = ImGuizmo::TRANSLATE;
    /* 선택 오브젝트 편집에 사용할 현재 트랜스폼 좌표계. */
    ImGuizmo::MODE _gizmoMode = ImGuizmo::LOCAL;
    /* MRT 디버그 전체 드로우 영역 크기. */
    ImVec2 _mrtDebugDrawArea = ImVec2(0.f, 0.f);
    /* DrawArea 기본값을 씬 뷰 크기로 한 번만 동기화했는지 여부. */
    bool _mrtDebugInitialized = false;

public:
    static Shared<Scene_View> Create();
};

NS_END

