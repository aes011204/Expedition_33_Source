#include "Scene_View.h"

#include "EditorInstance.h"
#include "Camera.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "PartObject.h"
#include "RenderTarget.h"
#include "Transform_Inspector.h"
#include "Client_Defines.h"
#include "Spawn_Prefab.h"

//add
#include "MapStaticMesh.h"

namespace MainEditor::Internal
{
    static ImGuizmo::MODE Resolve_GizmoModeFromTransformSpace()
    {
        return Transform_Inspector::Get_Space() == Transform_Inspector::ETransformSpace::World
            ? ImGuizmo::WORLD
            : ImGuizmo::LOCAL;
    }

    static void Sync_TransformSpaceFromGizmoMode(ImGuizmo::MODE gizmoMode)
    {
        Transform_Inspector::Set_Space(
            gizmoMode == ImGuizmo::WORLD
                ? Transform_Inspector::ETransformSpace::World
                : Transform_Inspector::ETransformSpace::Local);
    }

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
        const ImVec2 panelSize = ImVec2(290.f, 34.f);
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

void Scene_View::Pre_Render()
{
    if (!Is_Open())
        return;

    Tick_DragPreview(ImGui::GetIO().DeltaTime);
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
    _gizmoMode = Internal::Resolve_GizmoModeFromTransformSpace();

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
    {
        _gizmoMode = ImGuizmo::LOCAL;
        Internal::Sync_TransformSpaceFromGizmoMode(_gizmoMode);
    }
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
    {
        _gizmoMode = ImGuizmo::WORLD;
        Internal::Sync_TransformSpaceFromGizmoMode(_gizmoMode);
    }
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

        const bool isMouseOverOverlay =
            Internal::Is_MouseOverViewportOverlay(_viewportCanvasMin, _viewportCanvasMax);
        const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
        const bool isPrefabDragging =
            activePayload != nullptr &&
            activePayload->IsDataType("CONTENT_BROWSER_PREFAB");

        // add
        const bool isModelDragging =
            activePayload != nullptr &&
            activePayload->IsDataType("CONTENT_BROWSER_MODEL");

        // fix isPrefabDragging -> (isPrefabDragging || isModelDragging)
        if ((isPrefabDragging || isModelDragging) && !isMouseOverOverlay)
            ImGui::InvisibleButton("SceneViewportCanvas", _viewportSize);
        else
            ImGui::Dummy(_viewportSize);

        const ImTextureID textureId =
            static_cast<ImTextureID>(reinterpret_cast<intptr_t>(pSRV));
        drawList->AddImage(textureId, imageRect.imageMin, imageRect.imageMax);
        _viewportMin = imageRect.imageMin;
        _viewportMax = imageRect.imageMax;

        // add for mouse picking
        if (!ImGuizmo::IsOver() && !ImGuizmo::IsUsing() && _isHovered)
        {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                const ImVec2 mousePos = ImGui::GetIO().MousePos;

                if (EDITOR->IsPlaying())
                {
                    // Play 중 — 피킹 없음
                }
                else if (EDITOR->IsPaused())
                {
                    // Pause 중 — 피킹 허용
                    EDITOR->Request_Pick(mousePos, _viewportMin, _viewportMax);
                }
                else
                {
                    // Edit 모드 — 피킹 허용
                    EDITOR->Request_Pick(mousePos, _viewportMin, _viewportMax);
                }
            }
        }
        //

        // fix isPrefabDragging -> (isPrefabDragging || isModelDragging)
        if ((isPrefabDragging || isModelDragging) && !isMouseOverOverlay && ImGui::BeginDragDropTarget())
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

                _dragPreviewPrefabName = prefabName;
                _dragPreviewPixelPos = pixelPos;
                _dragPreviewRequestPending = true;

                if (payload->IsDelivery())
                {
                    if (SUCCEEDED(GAME->Update_Picking()))
                    {
                        Vec3 worldPos{};
                        if (GAME->Pick_WorldPosition(pixelPos, worldPos))
                        {
                            worldPos.y += 0.1f;

                            Shared<GameObject> spawned = Client::Spawn_Prefab::SpawnAt(
                                prefabName,
                                GAME->Current_LevelIndex(),
                                TEXT("Layer_GameObject"),
                                worldPos);

                            if (spawned)
                                EDITOR->Select_Object(spawned);
                        }
                    }

                    Clear_DragPreviewObject();
                }
            }
            // add
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                "CONTENT_BROWSER_MODEL",
                ImGuiDragDropFlags_AcceptBeforeDelivery))
            {
                const char* modelPathA = static_cast<const char*>(payload->Data);

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

                if (payload->IsDelivery())
                {
                    Vec3 worldPos = Vec3::Zero;
                    if (SUCCEEDED(GAME->Update_Picking()))
                        GAME->Pick_WorldPosition(pixelPos, worldPos);

                    auto model = ModelCom::Create(
                        EDITOR->Get_Device(), EDITOR->Get_Context(),
                        ModelType::NonAnim, modelPathA, Matrix::CreateScale(0.01f));

                    if (model)
                    {
                        Client::MapStaticMesh::MAPSTATICMESH_DESC desc{};
                        desc.model = model;
                        desc.position = worldPos;

                        Shared<GameObject> spawned;
                        if (SUCCEEDED(GAME->Add_GameObject(
                            ETOI(LevelType::Static), TEXT("GameObject_MapStaticMesh"),
                            GAME->Current_LevelIndex(), L"Layer_MapStaticMesh",
                            spawned, &desc)))
                        {
                            EDITOR->Select_Object(spawned);
                        }
                    }
                }
            }
            //

            ImGui::EndDragDropTarget();
        }
    }
    else
    {
        drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 20, 24, 255));
        drawList->AddRect(canvasMin, canvasMax, IM_COL32(55, 60, 70, 255));

        const bool isMouseOverOverlay =
            Internal::Is_MouseOverViewportOverlay(_viewportCanvasMin, _viewportCanvasMax);
        const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
        const bool isPrefabDragging =
            activePayload != nullptr &&
            activePayload->IsDataType("CONTENT_BROWSER_PREFAB");

        // add
        const bool isModelDragging =
            activePayload != nullptr &&
            activePayload->IsDataType("CONTENT_BROWSER_MODEL");

        // fix isPrefabDragging -> (isPrefabDragging || isModelDragging)
        if ((isPrefabDragging || isModelDragging) && !isMouseOverOverlay)
            ImGui::InvisibleButton("SceneViewportCanvas", _viewportSize);
        else
            ImGui::Dummy(_viewportSize);

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
    _gizmoMode = Internal::Resolve_GizmoModeFromTransformSpace();

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

    Shared<PartObject> selectedPartObject = dynamic_pointer_cast<PartObject>(selectedObject);
    Matrix worldMatrix = selectedPartObject ? selectedPartObject->Get_GizmoWorldMatrix() : transform->Get_WorldMatrix();

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetRect(_viewportMin.x, _viewportMin.y, viewportWidth, viewportHeight);

    // 알트 + 드래그로 오브젝트 복사
    const bool altPressed = ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt);

    if (altPressed && ImGuizmo::IsOver() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !_altDuplicateActive)
    {
        Shared<GameObject> sourceObject = selectedObject;
        Shared<TransformCom> sourceTransform = transform;

        if (sourceObject == nullptr || sourceTransform == nullptr)
            return;

        const Vec3 sourcePosition = sourceTransform->Get_WorldPosition();
        const Quat sourceRotation = sourceTransform->Get_RotationQuaternion();
        const Vec3 sourceScale = sourceTransform->Get_Scale();

        Shared<GameObject> duplicatedObject = EDITOR->Duplicate_SelectedObject();
        if (duplicatedObject == nullptr)
            return;

        Shared<TransformCom> duplicatedTransform = duplicatedObject->Get_Transform();
        if (duplicatedTransform == nullptr)
            return;

        // 복제본이 클래스 기본 위치가 아니라 원본 위치에서 시작하도록 세팅 -> 이거 안하면, C++ 코드에서 세팅된 Position으로 이동된다.
        duplicatedTransform->Set_WorldPosition(sourcePosition);
        duplicatedTransform->Set_RotationQuaternion(sourceRotation);
        duplicatedTransform->Set_Scale(sourceScale);

        EDITOR->Select_Object(duplicatedObject);

        selectedObject = duplicatedObject;
        transform = duplicatedTransform;
        worldMatrix = transform->Get_WorldMatrix();

        _altDuplicateActive = true;
    }

    const bool manipulated = ImGuizmo::Manipulate(
    reinterpret_cast<float*>(const_cast<Matrix*>(viewMatrix)),
    reinterpret_cast<float*>(const_cast<Matrix*>(projMatrix)),
    _gizmoOperation,
    _gizmoMode,
    reinterpret_cast<float*>(&worldMatrix));

    const bool isUsing = ImGuizmo::IsUsing();

    if (!altPressed || !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        _altDuplicateActive = false;

    if (!manipulated)
        return;

    if (selectedPartObject != nullptr)
    {
        selectedPartObject->Apply_GizmoWorldMatrix(worldMatrix);
        return;
    }

    Vec3 scale = Vec3::One;
    Quat rotation = Quat::Identity;
    Vec3 translation = Vec3::Zero;

    if (!worldMatrix.Decompose(scale, rotation, translation))
        return;

    transform->Set_WorldPosition(translation);
    transform->Set_RotationQuaternion(rotation);
    transform->Set_Scale(scale);
}

void Scene_View::Set_DragPreviewObject(const string& prefabName)
{
    if (_dragPreviewObject != nullptr && _dragPreviewPrefabName == prefabName)
        return;

    _dragPreviewObject.reset();
    _dragPreviewPrefabName = prefabName;

    if (prefabName.empty())
        return;

    // 프리펩 경로 가져와서, 게임오브젝트 생성.
    const fs::path prefabPath = Spawn_Prefab::Resolve_PrefabPath(prefabName);
    _dragPreviewObject = GAME->Instantiate_GameObjectJson(prefabPath);
}

void Scene_View::Clear_DragPreviewObject()
{
    _dragPreviewObject.reset();
    _dragPreviewPrefabName.clear();
    _dragPreviewRequestPending = false;
    _dragPreviewHasWorldPos = false;
}

void Scene_View::Tick_DragPreview(float timeDelta)
{
    if (!_dragPreviewRequestPending)
    {
        Clear_DragPreviewObject();
        return;
    }

    _dragPreviewRequestPending = false;

    Set_DragPreviewObject(_dragPreviewPrefabName);
    if (_dragPreviewObject == nullptr)
    {
        _dragPreviewHasWorldPos = false;
        return;
    }

    if (FAILED(GAME->Update_Picking()))
    {
        _dragPreviewHasWorldPos = false;
        return;
    }

    Vec3 worldPos{};
    if (!GAME->Pick_WorldPosition(_dragPreviewPixelPos, worldPos))
    {
        _dragPreviewHasWorldPos = false;
        return;
    }

    worldPos.y += 0.1f;
    _dragPreviewWorldPos = worldPos;
    _dragPreviewHasWorldPos = true;

    Shared<TransformCom> transform = _dragPreviewObject->Get_Transform();
    if (transform == nullptr)
        return;

    transform->Set_WorldPosition(worldPos);

    // Scene drag preview should stay visually frozen; only refresh transform/render state.
    _dragPreviewObject->Priority_Update(0.f);
    _dragPreviewObject->Update(0.f);
    _dragPreviewObject->Late_Update(0.f);
}

Shared<Scene_View> Scene_View::Create()
{
    return make_shared<Scene_View>();
}
