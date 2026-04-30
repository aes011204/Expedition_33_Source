#pragma once

#include "EditorWindow.h"
#include "Editor_ImGuiInclude.h"

NS_BEGIN(MainEditor)

class Scene_View final : public EditorWindow
{
public:
    /* 에디터 카메라 렌더링과 기즈모 편집을 붙일 씬 뷰포트 창을 생성한다. */
    Scene_View();
    virtual ~Scene_View() = default;

public:
    /* ImGui 렌더 전에 씬 뷰포트의 임시 상태를 갱신한다. */
    virtual void Update(float fTimeDelta) override;

    /* 씬 뷰포트 영역과 기본 ImGuizmo 툴바를 그린다. */
    virtual void Render() override;

private:
    /* 트랜스폼 기즈모 모드를 바꾸는 간단한 툴바를 그린다. */
    void Render_GizmoToolbar();

    /* 렌더 타겟이 연결되기 전까지 사용할 임시 뷰포트 영역을 그린다. */
    void Render_ViewportCanvas();

private:
    ImVec2 _viewportSize = {};                      /* 마지막으로 표시된 씬 뷰포트 크기. */
    ImGuizmo::OPERATION _gizmoOperation = ImGuizmo::TRANSLATE; /* 씬 기즈모의 현재 트랜스폼 동작. */
    ImGuizmo::MODE _gizmoMode = ImGuizmo::LOCAL;    /* 선택 오브젝트 편집에 사용할 현재 트랜스폼 좌표계. */
};

NS_END

