    #include "Scene_View.h"

#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "RenderTarget.h"

Scene_View::Scene_View()
    : Editor_Window{ L"Scene" }
{
}

void Scene_View::Update(float fTimeDelta)
{
}

void Scene_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (ImGui::Begin(windowName.c_str(), &isOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        Set_Open(isOpen);
        ImGuizmo::BeginFrame();
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

        Render_GizmoToolbar();
        ImGui::Separator();
        Render_ViewportCanvas();
    }
    else
    {
        Set_Open(isOpen);
    }

    ImGui::End();
}

void Scene_View::Render_GizmoToolbar()
{
    ImGui::TextDisabled("Tool");
    ImGui::SameLine();

    if (ImGui::RadioButton("Translate", _gizmoOperation == ImGuizmo::TRANSLATE))
        _gizmoOperation = ImGuizmo::TRANSLATE;

    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", _gizmoOperation == ImGuizmo::ROTATE))
        _gizmoOperation = ImGuizmo::ROTATE;

    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", _gizmoOperation == ImGuizmo::SCALE))
        _gizmoOperation = ImGuizmo::SCALE;

    ImGui::SameLine(0.f, 16.f);
    ImGui::TextDisabled("|");
    ImGui::SameLine(0.f, 12.f);

    ImGui::TextDisabled("Space");
    ImGui::SameLine();

    if (ImGui::RadioButton("Local", _gizmoMode == ImGuizmo::LOCAL))
        _gizmoMode = ImGuizmo::LOCAL;

    ImGui::SameLine();
    if (ImGui::RadioButton("World", _gizmoMode == ImGuizmo::WORLD))
        _gizmoMode = ImGuizmo::WORLD;
}

void Scene_View::Render_ViewportCanvas()
{
    _viewportSize = ImGui::GetContentRegionAvail();

    if (_viewportSize.x < 1.f)
        _viewportSize.x = 1.f;
    if (_viewportSize.y < 1.f)
        _viewportSize.y = 1.f;

    const uint32 width = static_cast<uint32>(_viewportSize.x);
    const uint32 height = static_cast<uint32>(_viewportSize.y);

    EffectEditorInstance::GetInstance()->Request_ViewportSize(
        EditorViewportTarget::Scene,
        width,
        height);

    ID3D11ShaderResourceView* pSRV = EffectEditorInstance::GetInstance()->Get_SharedViewportSRV();
    if (nullptr != pSRV)
    {
        const ImTextureID textureId =
            static_cast<ImTextureID>(reinterpret_cast<intptr_t>(pSRV));

        ImGui::Image(textureId, _viewportSize);
    }
    else
    {
        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        const ImVec2 canvasEnd = ImVec2(canvasPos.x + _viewportSize.x, canvasPos.y + _viewportSize.y);
        ImDrawList* pDrawList = ImGui::GetWindowDrawList();

        pDrawList->AddRectFilled(canvasPos, canvasEnd, IM_COL32(34, 38, 44, 255));
        pDrawList->AddRect(canvasPos, canvasEnd, IM_COL32(85, 95, 110, 255));
        pDrawList->AddText(ImVec2(canvasPos.x + 12.f, canvasPos.y + 12.f), IM_COL32(210, 216, 224, 255), "Scene RenderTarget Missing");

        ImGui::InvisibleButton("SceneViewportCanvas", _viewportSize);
    }
}


Shared<Scene_View> Scene_View::Create()
{
    return make_shared<Scene_View>();
}



