#pragma once

#include "Editor_Window.h"

NS_BEGIN(EffectEditor)

class Scene_View final : public Editor_Window
{
public:
    // 에디터 카메라 렌더링과 기즈모 편집을 붙일 씬 뷰포트 창을 생성한다.
    Scene_View();
    ~Scene_View() override;

public:
    void Update(float fTimeDelta) override;
    void Render() override;

private:
    void Render_GizmoToolbar();   // 트랜스폼 기즈모 모드를 바꾸는 간단한 툴바를 그린다.
    void Render_ViewportCanvas(); // 렌더 타겟이 연결되기 전까지 사용할 임시 뷰포트 영역을 그린다.

private:
    // 마지막으로 표시된 씬 뷰포트 크기.
    ImVec2 _viewportSize{};
    // 씬 기즈모의 현재 트랜스폼 동작.
    ImGuizmo::OPERATION _gizmoOperation{ ImGuizmo::TRANSLATE };
    // 선택 오브젝트 편집에 사용할 현재 트랜스폼 좌표계.
    ImGuizmo::MODE _gizmoMode{ ImGuizmo::LOCAL };

public:
    static Shared<Scene_View> Create();
    void Free() override;
};

NS_END
