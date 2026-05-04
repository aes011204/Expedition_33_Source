#include "Scene_View.h"

#include "EditorInstance.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "RenderTarget.h"

Scene_View::Scene_View()
    : Editor_Window{ L"Scene" }
{
}

void Scene_View::Update(float timeDelta)
{
    Handle_GizmoShortcut();
}

void Scene_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (EDITOR->Consume_WindowFocusRequest(EDITOR_VIEWPORT_TARGET::Scene))
        ImGui::SetNextWindowFocus();

    if (ImGui::Begin(windowName.c_str(), &isOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        Set_Open(isOpen);
        ImGuizmo::BeginFrame();
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

        Render_ViewportCanvas();
        Handle_FocusRequest();
        Render_SelectedObjectGizmo();
        Render_GizmoOverlay();
    }
    else
    {
        Set_Open(isOpen);
    }

    ImGui::End();
}

void Scene_View::Handle_GizmoShortcut()
{
    if (!_isFocused)
        return;

    if (GAME->KeyPress(KEY_TYPE::RBUTTON))
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_W, false))
    {
        _gizmoOperation = ImGuizmo::TRANSLATE;
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_E, false))
    {
        _gizmoOperation = ImGuizmo::ROTATE;
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_R, false))
    {
        _gizmoOperation = ImGuizmo::SCALE;
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
    {
        _gizmoMode = (_gizmoMode == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
        return;
    }
}

void Scene_View::Render_GizmoOverlay()
{
    const float viewportWidth = _viewportMax.x - _viewportMin.x;
    const float viewportHeight = _viewportMax.y - _viewportMin.y;
    if (viewportWidth <= 1.f || viewportHeight <= 1.f)
        return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const ImVec2 panelSize = ImVec2(232.f, 34.f);
    const ImVec2 panelMin = ImVec2(_viewportMax.x - panelSize.x - 16.f, _viewportMin.y + 16.f);
    const ImVec2 panelMax = ImVec2(panelMin.x + panelSize.x, panelMin.y + panelSize.y);

    drawList->AddRectFilled(panelMin, panelMax, IM_COL32(36, 38, 42, 220), 6.f);
    drawList->AddRect(panelMin, panelMax, IM_COL32(92, 96, 104, 255), 6.f);

    ImGui::SetCursorScreenPos(ImVec2(panelMin.x + 8.f, panelMin.y + 5.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.f, 3.f));

    const ImVec4 activeButtonColor = ImVec4(0.79f, 0.56f, 0.18f, 1.f);
    const ImVec4 activeHoveredColor = ImVec4(0.88f, 0.64f, 0.23f, 1.f);
    const ImVec4 activeTextColor = ImVec4(0.09f, 0.09f, 0.09f, 1.f);

    const bool isTranslate = (_gizmoOperation == ImGuizmo::TRANSLATE);
    const bool isRotate = (_gizmoOperation == ImGuizmo::ROTATE);
    const bool isScale = (_gizmoOperation == ImGuizmo::SCALE);
    const bool isLocal = (_gizmoMode == ImGuizmo::LOCAL);
    const bool isWorld = (_gizmoMode == ImGuizmo::WORLD);

    if (isTranslate)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_Text, activeTextColor);
    }
    if (ImGui::Button("W", ImVec2(24.f, 22.f)))
        _gizmoOperation = ImGuizmo::TRANSLATE;
    if (isTranslate)
        ImGui::PopStyleColor(3);

    ImGui::SameLine();

    if (isRotate)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_Text, activeTextColor);
    }
    if (ImGui::Button("E", ImVec2(24.f, 22.f)))
        _gizmoOperation = ImGuizmo::ROTATE;
    if (isRotate)
        ImGui::PopStyleColor(3);

    ImGui::SameLine();

    if (isScale)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_Text, activeTextColor);
    }
    if (ImGui::Button("R", ImVec2(24.f, 22.f)))
        _gizmoOperation = ImGuizmo::SCALE;
    if (isScale)
        ImGui::PopStyleColor(3);

    ImGui::SameLine(0.f, 8.f);

    if (isLocal)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_Text, activeTextColor);
    }
    if (ImGui::Button("Local", ImVec2(56.f, 22.f)))
        _gizmoMode = ImGuizmo::LOCAL;
    if (isLocal)
        ImGui::PopStyleColor(3);

    ImGui::SameLine();

    if (isWorld)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_Text, activeTextColor);
    }
    if (ImGui::Button("World", ImVec2(56.f, 22.f)))
        _gizmoMode = ImGuizmo::WORLD;
    if (isWorld)
        ImGui::PopStyleColor(3);

    ImGui::PopStyleVar();
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

    EditorInstance::GetInstance()->Request_ViewportSize(
        EDITOR_VIEWPORT_TARGET::Scene,
        width,
        height);

    ID3D11ShaderResourceView* pSRV = EditorInstance::GetInstance()->Get_SharedViewportSRV();
    if (nullptr != pSRV)
    {
        const ImTextureID textureId =
            static_cast<ImTextureID>(reinterpret_cast<intptr_t>(pSRV));

        ImGui::Image(textureId, _viewportSize);
        _viewportMin = ImGui::GetItemRectMin();
        _viewportMax = ImGui::GetItemRectMax();
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
        _viewportMin = ImGui::GetItemRectMin();
        _viewportMax = ImGui::GetItemRectMax();
    }
}

void Scene_View::Handle_FocusRequest()
{
    Shared<GameObject> targetObject = EDITOR->Consume_FocusObjectRequest();
    if (targetObject == nullptr)
        return;

    Shared<TransformCom> transform = targetObject->Get_Transform();
    if (transform == nullptr)
        return;

    const Vec3 focusTargetPosition = transform->Get_WorldPosition() + Vec3(0.f, 1.5f, -4.f);
    GAME->Focus_FreeCameraOnPosition(focusTargetPosition);
}

void Scene_View::Render_SelectedObjectGizmo()
{
    Shared<GameObject> selectedObject = EDITOR->Selected_Object();
    if (selectedObject == nullptr)
        return;

    Shared<TransformCom> transform = selectedObject->Get_Transform();
    if (transform == nullptr)
        return;

    const Matrix* viewMatrix = GAME->Get_Transform(D3DTS::View);
    const Matrix* projMatrix = GAME->Get_Transform(D3DTS::Proj);
    if (viewMatrix == nullptr || projMatrix == nullptr)
        return;

    const float viewportWidth = _viewportMax.x - _viewportMin.x;
    const float viewportHeight = _viewportMax.y - _viewportMin.y;
    if (viewportWidth <= 1.f || viewportHeight <= 1.f)
        return;

    Matrix worldMatrix = transform->Get_WorldMatrix();

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetRect(_viewportMin.x, _viewportMin.y, viewportWidth, viewportHeight);

    const bool manipulated = ImGuizmo::Manipulate(
        reinterpret_cast<float*>(const_cast<Matrix*>(viewMatrix)),
        reinterpret_cast<float*>(const_cast<Matrix*>(projMatrix)),
        _gizmoOperation,
        _gizmoMode,
        reinterpret_cast<float*>(&worldMatrix));

    if (!manipulated)
        return;

    Vec3 scale = Vec3::One;
    Quat rotation = Quat::Identity;
    Vec3 translation = Vec3::Zero;

    if (!worldMatrix.Decompose(scale, rotation, translation))
        return;

    transform->Set_WorldPosition(translation);
    transform->Set_RotationQuaternion(rotation);
    transform->Set_Scale(scale);
}

Shared<Scene_View> Scene_View::Create()
{
    return make_shared<Scene_View>();
}
