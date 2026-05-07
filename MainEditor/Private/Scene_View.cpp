#include "Scene_View.h"

#include "EditorInstance.h"
#include "Camera.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "RenderTarget.h"
#include "Client_Defines.h"
#include "Spawn_Prefab.h"

namespace MainEditor::Internal
{
struct ViewportImageRect
{
    ImVec2 imageMin;
    ImVec2 imageMax;
};

struct OverlayRect
{
    ImVec2 min;
    ImVec2 max;
};

static ViewportImageRect Compute_AspectFitRect(const ImVec2& canvasMin, const ImVec2& canvasSize, float aspect)
{
    if (aspect <= 0.f || canvasSize.x <= 1.f || canvasSize.y <= 1.f)
    {
        return {
            canvasMin,
            ImVec2(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y)
        };
    }

    float imageWidth = canvasSize.x;
    float imageHeight = imageWidth / aspect;

    if (imageHeight > canvasSize.y)
    {
        imageHeight = canvasSize.y;
        imageWidth = imageHeight * aspect;
    }

    const ImVec2 imageMin = ImVec2(
        canvasMin.x + (canvasSize.x - imageWidth) * 0.5f,
        canvasMin.y + (canvasSize.y - imageHeight) * 0.5f
    );

    return {
        imageMin,
        ImVec2(imageMin.x + imageWidth, imageMin.y + imageHeight)
    };
}

static OverlayRect Compute_GizmoOverlayRect(const ImVec2& viewportMin, const ImVec2& viewportMax)
{
    const ImVec2 panelSize = ImVec2(360.f, 34.f);
    const ImVec2 panelMin = ImVec2(viewportMax.x - panelSize.x - 16.f, viewportMin.y + 16.f);

    return {
        panelMin,
        ImVec2(panelMin.x + panelSize.x, panelMin.y + panelSize.y)
    };
}

static OverlayRect Compute_PreviewViewportOverlayRect(const ImVec2& viewportMin, const ImVec2& viewportMax)
{
    const ImVec2 panelSize = ImVec2(352.f, 34.f);
    const ImVec2 panelMin = ImVec2(viewportMax.x - panelSize.x - 16.f, viewportMin.y + 58.f);

    return {
        panelMin,
        ImVec2(panelMin.x + panelSize.x, panelMin.y + panelSize.y)
    };
}

static bool Is_PointInRect(const ImVec2& point, const OverlayRect& rect)
{
    return point.x >= rect.min.x && point.x <= rect.max.x &&
        point.y >= rect.min.y && point.y <= rect.max.y;
}

static bool Is_MouseOverViewportOverlay(const ImVec2& viewportMin, const ImVec2& viewportMax)
{
    const ImVec2 mousePos = ImGui::GetIO().MousePos;
    return Is_PointInRect(mousePos, Compute_GizmoOverlayRect(viewportMin, viewportMax)) ||
        Is_PointInRect(mousePos, Compute_PreviewViewportOverlayRect(viewportMin, viewportMax));
}
}

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
        
        Render_MRTDebugPopup();

        ImGui::Separator();
        Render_ViewportCanvas();
        Handle_FocusRequest();
        Render_SelectedObjectGizmo();
        Render_GizmoOverlay();
        Render_PreviewViewportOverlay();
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

    // 전환은 키보드 세팅 막아야할거같다.
    //if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
    //{
    //    _gizmoMode = (_gizmoMode == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
    //    return;
    //}
}

void Scene_View::Render_GizmoOverlay()
{
    const float viewportWidth = _viewportCanvasMax.x - _viewportCanvasMin.x;
    const float viewportHeight = _viewportCanvasMax.y - _viewportCanvasMin.y;
    if (viewportWidth <= 1.f || viewportHeight <= 1.f)
        return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const ImVec2 panelSize = ImVec2(360.f, 34.f);
    const ImVec2 panelMin = ImVec2(_viewportCanvasMax.x - panelSize.x - 16.f, _viewportCanvasMin.y + 16.f);
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

    Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    const bool isFreeCamera =
        activeCamera && activeCamera->Get_ObjectType() == ObjectType::FreeCamera;

    if (!isFreeCamera)
        ImGui::BeginDisabled();

    ImGui::TextUnformatted("Speed");
    ImGui::SameLine();

    if (isFreeCamera)
    {
        Shared<TransformCom> transform = activeCamera->Get_Transform();
        float speed = transform ? transform->Get_SpeedPerSec() : 0.f;

        ImGui::SetNextItemWidth(78.f);
        if (ImGui::DragFloat("##FreeCameraSpeed", &speed, 0.1f, 0.1f, 200.f, "%.2f"))
        {
            if (transform)
                transform->Set_SpeedPerSec(speed);
        }
    }
    else
    {
        ImGui::SetNextItemWidth(78.f);
        float dummySpeed = 0.f;
        ImGui::DragFloat("##FreeCameraSpeed", &dummySpeed, 0.f, 0.f, 0.f, "N/A");
    }

    if (!isFreeCamera)
        ImGui::EndDisabled();

    ImGui::SameLine(0.f, 10.f);

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

void Scene_View::Render_PreviewViewportOverlay()
{
    const float viewportWidth = _viewportCanvasMax.x - _viewportCanvasMin.x;
    const float viewportHeight = _viewportCanvasMax.y - _viewportCanvasMin.y;
    if (viewportWidth <= 1.f || viewportHeight <= 1.f)
        return;

    if (_pendingPreviewViewportWidth <= 0 || _pendingPreviewViewportHeight <= 0)
        Sync_PreviewViewportInput();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const Internal::OverlayRect panelRect = Internal::Compute_PreviewViewportOverlayRect(_viewportCanvasMin, _viewportCanvasMax);

    drawList->AddRectFilled(panelRect.min, panelRect.max, IM_COL32(36, 38, 42, 220), 6.f);
    drawList->AddRect(panelRect.min, panelRect.max, IM_COL32(92, 96, 104, 255), 6.f);

    ImGui::SetCursorScreenPos(ImVec2(panelRect.min.x + 8.f, panelRect.min.y + 5.f));
    ImGui::PushID("PreviewViewportOverlay");
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.f, 3.f));

    const ImVec4 activeButtonColor = ImVec4(0.79f, 0.56f, 0.18f, 1.f);
    const ImVec4 activeHoveredColor = ImVec4(0.88f, 0.64f, 0.23f, 1.f);
    const ImVec4 activeTextColor = ImVec4(0.09f, 0.09f, 0.09f, 1.f);
    const auto drawModeButton = [&](const char* label, const ImVec2& size, bool active)
    {
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoveredColor);
            ImGui::PushStyleColor(ImGuiCol_Text, activeTextColor);
        }

        const bool clicked = ImGui::Button(label, size);

        if (active)
            ImGui::PopStyleColor(3);

        return clicked;
    };

    drawModeButton("Manual", ImVec2(62.f, 22.f), true);

    ImGui::SameLine(0.f, 8.f);

    ImGui::TextUnformatted("W");
    ImGui::SameLine(0.f, 4.f);
    ImGui::SetNextItemWidth(48.f);
    ImGui::InputInt("##PreviewViewportWidth", &_pendingPreviewViewportWidth, 0, 0);

    ImGui::SameLine(0.f, 4.f);
    ImGui::TextUnformatted("x");

    ImGui::SameLine(0.f, 4.f);
    ImGui::TextUnformatted("H");
    ImGui::SameLine(0.f, 4.f);
    ImGui::SetNextItemWidth(48.f);
    ImGui::InputInt("##PreviewViewportHeight", &_pendingPreviewViewportHeight, 0, 0);

    ImGui::SameLine(0.f, 8.f);
    if (ImGui::Button("Apply", ImVec2(48.f, 22.f)))
    {
        const uint32 width = static_cast<uint32>(max(1, _pendingPreviewViewportWidth));
        const uint32 height = static_cast<uint32>(max(1, _pendingPreviewViewportHeight));

        _pendingPreviewViewportWidth = static_cast<int>(width);
        _pendingPreviewViewportHeight = static_cast<int>(height);
        EDITOR->Set_ManualPreviewViewportSize(width, height);
    }

    ImGui::PopStyleVar();
    ImGui::PopID();
}

void Scene_View::Sync_PreviewViewportInput()
{
    _pendingPreviewViewportWidth = static_cast<int>(GAME->Get_ViewportWidth());
    _pendingPreviewViewportHeight = static_cast<int>(GAME->Get_ViewportHeight());
}

void Scene_View::Render_MRTDebugPopup()
{
#ifdef _DEBUG
    const ImGuiStyle& style = ImGui::GetStyle();
    const float buttonWidth = ImGui::CalcTextSize("MRT").x + style.FramePadding.x * 2.f;
    const float buttonPosX = ImGui::GetContentRegionMax().x - buttonWidth;

    ImGui::SameLine();
    if (buttonPosX > ImGui::GetCursorPosX())
        ImGui::SetCursorPosX(buttonPosX);

    if (ImGui::Button("MRT"))
        ImGui::OpenPopup("SceneMRTDebugPopup");

    const auto& rtStates = GAME->Get_RTs();

    if (!ImGui::BeginPopup("SceneMRTDebugPopup"))
        return;

    if (rtStates.empty())
    {
        ImGui::TextDisabled("No RT");
        ImGui::EndPopup();
        return;
    }

    bool isAllDisplayed = true;
    for (const auto& [_, isVisible] : rtStates)
        isAllDisplayed = isAllDisplayed && isVisible;

    if (ImGui::Checkbox("Display", &isAllDisplayed))
        GAME->Display_AllRTs(isAllDisplayed);

    if (ImGui::DragFloat2("Size", &_mrtDebugDrawArea.x, 1.f, 1.f, 4096.f, "%.0f"))
    {
        _mrtDebugDrawArea.x = max(_mrtDebugDrawArea.x, 1.f);
        _mrtDebugDrawArea.y = max(_mrtDebugDrawArea.y, 1.f);
        GAME->Resize_DrawArea(_mrtDebugDrawArea.x, _mrtDebugDrawArea.y);
    }

    ImGui::Separator();

    for (const auto& [targetTag, isVisible] : rtStates)
    {
        bool isDisplayed = isVisible;
        const string label = String::ToString(targetTag);

        ImGui::PushID(label.c_str());
        if (ImGui::Checkbox("##Display", &isDisplayed))
            GAME->Display_RT(targetTag, isDisplayed);
        ImGui::SameLine();
        ImGui::TextUnformatted(label.c_str());
        ImGui::PopID();
    }

    ImGui::EndPopup();
#else
    (void)0;
#endif
}

void Scene_View::Render_ViewportCanvas()
{
    _viewportSize = ImGui::GetContentRegionAvail();

    if (_viewportSize.x < 1.f)
        _viewportSize.x = 1.f;
    if (_viewportSize.y < 1.f)
        _viewportSize.y = 1.f;

#ifdef _DEBUG
    if (!_mrtDebugInitialized)
    {
        _mrtDebugDrawArea = _viewportSize;
        GAME->Resize_DrawArea(_mrtDebugDrawArea.x, _mrtDebugDrawArea.y);
        _mrtDebugInitialized = true;
    }
#endif

    const uint32 width = static_cast<uint32>(_viewportSize.x);
    const uint32 height = static_cast<uint32>(_viewportSize.y);

    EditorInstance::GetInstance()->Request_ViewportSize(
        EDITOR_VIEWPORT_TARGET::Scene,
        width,
        height);

    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasMax = ImVec2(canvasMin.x + _viewportSize.x, canvasMin.y + _viewportSize.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    _viewportCanvasMin = canvasMin;
    _viewportCanvasMax = canvasMax;

    ID3D11ShaderResourceView* pSRV = EditorInstance::GetInstance()->Get_SharedViewportSRV();
    if (nullptr != pSRV)
    {
        const float aspect = EDITOR->Get_EffectiveViewportAspect();
        const Internal::ViewportImageRect imageRect =
            Internal::Compute_AspectFitRect(canvasMin, _viewportSize, aspect);

        drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 20, 24, 255));
        drawList->AddRect(canvasMin, canvasMax, IM_COL32(55, 60, 70, 255));

        if (Internal::Is_MouseOverViewportOverlay(_viewportCanvasMin, _viewportCanvasMax))
            ImGui::Dummy(_viewportSize);
        else
            ImGui::InvisibleButton("SceneViewportCanvas", _viewportSize);

        const ImTextureID textureId =
            static_cast<ImTextureID>(reinterpret_cast<intptr_t>(pSRV));
        drawList->AddImage(textureId, imageRect.imageMin, imageRect.imageMax);
        _viewportMin = imageRect.imageMin;
        _viewportMax = imageRect.imageMax;

        if (ImGui::BeginDragDropTarget())
        {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                    "CONTENT_BROWSER_PREFAB",
                    ImGuiDragDropFlags_AcceptBeforeDelivery))
                {
                    const char* prefabName = static_cast<const char*>(payload->Data);
                    const float viewportWidth = _viewportMax.x - _viewportMin.x;
                    const float viewportHeight = _viewportMax.y - _viewportMin.y;
                    const float depthWidth = static_cast<float>(GAME->Get_ViewportWidth());
                    const float depthHeight = static_cast<float>(GAME->Get_ViewportHeight());

                    POINT pixelPos{};
                    pixelPos.x = static_cast<LONG>(
                        (ImGui::GetMousePos().x - _viewportMin.x) *
                        (depthWidth / max(viewportWidth, 1.f)));
                    pixelPos.y = static_cast<LONG>(
                        (ImGui::GetMousePos().y - _viewportMin.y) *
                        (depthHeight / max(viewportHeight, 1.f)));

                    Vec3 worldPos{};
                    if (GAME->Pick_WorldPosition(pixelPos, worldPos))
                    {
                    worldPos.y += 0.1f;

                    DebugBoxDesc previewBox{};
                    previewBox.center = worldPos;
                    previewBox.extents = Vec3(0.35f, 0.35f, 0.35f);
                    previewBox.style.color = Color(0.2f, 0.9f, 0.4f, 1.f);
                    previewBox.style.duration = 0.f;
                    previewBox.style.depthEnabled = false;
                    GAME->Draw_DebugBox(previewBox);

                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    const ImVec2 textPos = ImVec2(_viewportMin.x + 12.f, _viewportMin.y + 12.f);
                    const string previewLabel = "Drop Prefab : " + string(prefabName);
                    drawList->AddText(textPos, IM_COL32(120, 255, 170, 255), previewLabel.c_str());

                    if (payload->IsDelivery())
                    {
                        Shared<GameObject> spawned = Client::Spawn_Prefab::SpawnAt(
                            prefabName,
                            GAME->Current_LevelIndex(),
                            TEXT("Layer_GameObject"),
                            worldPos);

                        if (spawned)
                        {
                            EDITOR->Select_Object(spawned);
                        }
                    }
                }
            }

            ImGui::EndDragDropTarget();
        }
    }
    else
    {
        drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 20, 24, 255));
        drawList->AddRect(canvasMin, canvasMax, IM_COL32(55, 60, 70, 255));

        if (Internal::Is_MouseOverViewportOverlay(_viewportCanvasMin, _viewportCanvasMax))
            ImGui::Dummy(_viewportSize);
        else
            ImGui::InvisibleButton("SceneViewportCanvas", _viewportSize);

        drawList->AddText(ImVec2(canvasMin.x + 12.f, canvasMin.y + 12.f), IM_COL32(210, 216, 224, 255), "Scene RenderTarget Missing");
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

    const bool altPressed = ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt);
    if (altPressed && ImGuizmo::IsOver() && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !_altDuplicateActive)
    {
        Shared<GameObject> duplicatedObject = EDITOR->Duplicate_SelectedObject();
        if (duplicatedObject == nullptr)
            return;

        selectedObject = duplicatedObject;
        transform = selectedObject->Get_Transform();
        if (transform == nullptr)
            return;

        worldMatrix = transform->Get_WorldMatrix();
        _altDuplicateActive = true;
    }

    const bool manipulated = ImGuizmo::Manipulate(
        reinterpret_cast<float*>(const_cast<Matrix*>(viewMatrix)),
        reinterpret_cast<float*>(const_cast<Matrix*>(projMatrix)),
        _gizmoOperation,
        _gizmoMode,
        reinterpret_cast<float*>(&worldMatrix));

    if (!altPressed || !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        _altDuplicateActive = false;

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
