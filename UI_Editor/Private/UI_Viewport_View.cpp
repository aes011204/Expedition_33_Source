#include "pch.h"
#include "UI_Viewport_View.h"
#include "UI_Editor_Manager.h"
#include "UI_Editor_Utils.h"

namespace UIEditor
{
    namespace
    {
        constexpr float kMinViewportZoomFactor = 1.f;
        constexpr float kMaxViewportZoomFactor = 4.f;
        constexpr float kViewportZoomStep = 1.1f;
    }

    UI_Viewport_View::UI_Viewport_View(UI_Editor_Manager& editorManager)
        : UI_Editor_Window{ L"Viewport" }
        , _editorManager{ editorManager }
    {
    }

    void UI_Viewport_View::Render()
    {
        if (!Is_Open())
            return;

        bool isOpen = Is_Open();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        if (ImGui::Begin("Viewport", &isOpen, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

            ImGui::SetCursorPosX(8.f);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Zoom: %.0f%%", _zoomFactor * 100.f);
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset Zoom"))
                _zoomFactor = 1.f;
            ImGui::Separator();

            ImVec2 available = ImGui::GetContentRegionAvail();
            available.x = max(1.f, available.x);
            available.y = max(1.f, available.y);

            UI_Document& document = _editorManager.Get_Document();
            const ImVec2 canvasSize = document.Get_CanvasSize();

            ImGuiIO& io = ImGui::GetIO();
            const bool canZoomViewport = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            if (canZoomViewport && io.MouseWheel != 0.f)
            {
                if (io.MouseWheel > 0.f)
                {
                    for (int32 i = 0; i < static_cast<int32>(io.MouseWheel); ++i)
                        _zoomFactor *= kViewportZoomStep;
                }
                else
                {
                    for (int32 i = 0; i < static_cast<int32>(-io.MouseWheel); ++i)
                        _zoomFactor /= kViewportZoomStep;
                }
            }
            _zoomFactor = min(max(_zoomFactor, kMinViewportZoomFactor), kMaxViewportZoomFactor);

            const float canvasWidth = max(1.f, canvasSize.x);
            const float canvasHeight = max(1.f, canvasSize.y);
            const ImVec2 canvasScreenSize = ImVec2(available.x * _zoomFactor, available.y * _zoomFactor);
            const float baseScaleX = available.x / canvasWidth;
            const float baseScaleY = available.y / canvasHeight;
            const float displayScaleX = baseScaleX * _zoomFactor;
            const float displayScaleY = baseScaleY * _zoomFactor;

            const float gridSize = 16.f;

            const ImVec2 contentCursorPos = ImGui::GetCursorPos();
            const ImVec2 viewAreaMin = ImGui::GetCursorScreenPos();
            const ImVec2 viewAreaMax = ImVec2(viewAreaMin.x + available.x, viewAreaMin.y + available.y);
            ImGui::SetCursorPos(contentCursorPos);
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            const ImVec2 viewportMin = viewAreaMin;
            const ImVec2 viewportMax = viewAreaMax;
            ImVec2 canvasMax = ImVec2(canvasPos.x + canvasScreenSize.x, canvasPos.y + canvasScreenSize.y);

            const ImU32 viewportColor = IM_COL32(15, 17, 20, 255);
            const ImU32 canvasColor = IM_COL32(26, 28, 32, 255);
            const ImU32 gridColor = IM_COL32(52, 56, 64, 160);
            const ImU32 borderColor = IM_COL32(92, 100, 112, 255);
            const ImU32 canvasBoundaryColor = IM_COL32(150, 160, 174, 220);

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            drawList->AddRectFilled(viewportMin, viewportMax, viewportColor);
            drawList->AddRectFilled(canvasPos, canvasMax, canvasColor);

            for (float x = 0.f; x <= canvasSize.x; x += gridSize)
            {
                float screenX = canvasPos.x + x * displayScaleX;
                drawList->AddLine(ImVec2(screenX, canvasPos.y), ImVec2(screenX, canvasMax.y), gridColor);
            }
            for (float y = 0.f; y <= canvasSize.y; y += gridSize)
            {
                float screenY = canvasPos.y + y * displayScaleY;
                drawList->AddLine(ImVec2(canvasPos.x, screenY), ImVec2(canvasMax.x, screenY), gridColor);
            }

            ImGui::InvisibleButton("##UIEditorViewportCanvas", canvasScreenSize, ImGuiButtonFlags_MouseButtonLeft);
            const bool isViewportHovered = ImGui::IsItemHovered();
            const bool isViewportActive = ImGui::IsItemActive();

            const bool canPanViewport = _zoomFactor > kMinViewportZoomFactor && (isViewportHovered || _isPanningViewport);
            if (!_isPanningViewport && canPanViewport && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                _isPanningViewport = true;

            if (_isPanningViewport)
            {
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) || _zoomFactor <= kMinViewportZoomFactor)
                {
                    _isPanningViewport = false;
                }
                else
                {
                    const float nextScrollX = min(max(ImGui::GetScrollX() - io.MouseDelta.x, 0.f), ImGui::GetScrollMaxX());
                    const float nextScrollY = min(max(ImGui::GetScrollY() - io.MouseDelta.y, 0.f), ImGui::GetScrollMaxY());
                    ImGui::SetScrollX(nextScrollX);
                    ImGui::SetScrollY(nextScrollY);
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                }
            }

            const ImVec2 mousePos = ImGui::GetMousePos();
            const ImVec2 mouseCanvasPos = ImVec2((mousePos.x - canvasPos.x) / displayScaleX, (mousePos.y - canvasPos.y) / displayScaleY);

            const vector<UI_Object>& objects = document.Get_Objects();
            vector<const UI_Object*> sortedObjects;
            sortedObjects.reserve(objects.size());
            for (const UI_Object& object : objects)
                sortedObjects.push_back(&object);

            stable_sort(
                sortedObjects.begin(),
                sortedObjects.end(),
                [](const UI_Object* lhs, const UI_Object* rhs)
                {
                    if (lhs->sortOrder != rhs->sortOrder)
                        return lhs->sortOrder < rhs->sortOrder;

                    return lhs->id < rhs->id;
                });

            for (const UI_Object* objectPtr : sortedObjects)
            {
                const UI_Object& object = *objectPtr;
                const ImVec2 objectSize = ImVec2(object.size.x * object.scale.x, object.size.y * object.scale.y);
                const ImVec2 objectMin = ImVec2(canvasPos.x + object.position.x * displayScaleX, canvasPos.y + object.position.y * displayScaleY);
                const ImVec2 objectMax = ImVec2(objectMin.x + objectSize.x * displayScaleX, objectMin.y + objectSize.y * displayScaleY);
                const bool isSelected = (_editorManager.Get_SelectedObjectId() == object.id);

                const ImU32 fillColor = Get_ObjectFillColor(object.type);
                const ImU32 objectBorderColor = isSelected ? IM_COL32(255, 217, 102, 255) : IM_COL32(210, 214, 220, 220);
                const float borderThickness = isSelected ? 2.5f : 1.5f;
                ID3D11ShaderResourceView* previewSRV = nullptr;

                if (object.type == UI_ObjectType::Image && !object.textureGuid.empty())
                    previewSRV = _editorManager.Get_TexturePreviewSRV(object.textureGuid);

                const bool hasTextContent = object.type == UI_ObjectType::Text && !object.text.empty();

                if (nullptr != previewSRV)
                    drawList->AddImage(To_ImTextureID(previewSRV), objectMin, objectMax);
                else if (!hasTextContent)
                    drawList->AddRectFilled(objectMin, objectMax, fillColor);

                drawList->AddRect(objectMin, objectMax, objectBorderColor, 0.f, 0, borderThickness);

                bool usesSpriteFontPreview = false;
                if (object.type == UI_ObjectType::Text && (!object.fontGuid.empty() || !object.fontPath.empty()) && !object.text.empty())
                {
                    D3D11_RECT clipRect = {};
                    if (Build_ScreenClipRect(objectMin, objectMax, clipRect))
                    {
                        usesSpriteFontPreview = _editorManager.Queue_FontPreview(
                            object.fontGuid,
                            object.fontPath,
                            String::ToWString(object.text),
                            Vec2(objectMin.x + 4.f, objectMin.y + 3.f),
                            Vec4(object.color.x, object.color.y, object.color.z, object.color.w),
                            object.rotationRadians,
                            Vec2(0.f, 0.f),
                            (max(4.f, object.fontSize) / 32.f) * max(0.01f, object.scale.x) * displayScaleX,
                            clipRect);
                    }
                }

                if (!usesSpriteFontPreview && nullptr == previewSRV && objectMax.x - objectMin.x > 32.f && objectMax.y - objectMin.y > 18.f)
                {
                    const string previewText = object.type == UI_ObjectType::Text && !object.text.empty() ? object.text : object.name;
                    const ImU32 previewTextColor = object.type == UI_ObjectType::Text ? ImGui::ColorConvertFloat4ToU32(object.color) : IM_COL32(245, 247, 250, 230);
                    const ImVec2 textPosition = ImVec2(objectMin.x + 4.f, objectMin.y + 3.f);

                    if (object.type == UI_ObjectType::Text)
                        drawList->AddText(ImGui::GetFont(), max(4.f, object.fontSize) * max(0.01f, object.scale.x) * displayScaleX, textPosition, previewTextColor, previewText.c_str());
                    else
                        drawList->AddText(textPosition, previewTextColor, previewText.c_str());

                    if (object.type == UI_ObjectType::Text && isSelected && !object.fontTag.empty() && objectMax.y - objectMin.y > 34.f)
                    {
                        const string fontPreviewText = "Font: " + object.fontTag;
                        drawList->AddText(ImVec2(objectMin.x + 4.f, objectMax.y - 17.f), IM_COL32(255, 217, 102, 240), fontPreviewText.c_str());
                    }
                }
            }

            if (isViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                ObjectId pickedObjectId = 0;
                for (auto iter = sortedObjects.rbegin(); iter != sortedObjects.rend(); ++iter)
                {
                    const UI_Object& object = **iter;
                    const ImVec2 objectSize = ImVec2(object.size.x * object.scale.x, object.size.y * object.scale.y);
                    const bool isInsideObject =
                        mouseCanvasPos.x >= object.position.x && mouseCanvasPos.x <= object.position.x + objectSize.x &&
                        mouseCanvasPos.y >= object.position.y && mouseCanvasPos.y <= object.position.y + objectSize.y;

                    if (isInsideObject)
                    {
                        pickedObjectId = object.id;
                        break;
                    }
                }

                _editorManager.Set_SelectedObjectId(pickedObjectId);
                _draggingObjectId = pickedObjectId;
                _dragStartMouseCanvasPos = mouseCanvasPos;

                const UI_Object* pickedObject = document.Find_Object(pickedObjectId);
                if (nullptr != pickedObject)
                    _dragStartObjectPosition = pickedObject->position;
            }

            if (_draggingObjectId != 0 && isViewportActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.f))
            {
                UI_Object* draggingObject = document.Find_Object(_draggingObjectId);
                if (nullptr != draggingObject)
                {
                    const ImVec2 delta = ImVec2(
                        mouseCanvasPos.x - _dragStartMouseCanvasPos.x,
                        mouseCanvasPos.y - _dragStartMouseCanvasPos.y);

                    draggingObject->position = ImVec2(
                        _dragStartObjectPosition.x + delta.x,
                        _dragStartObjectPosition.y + delta.y);
                    draggingObject->position = Clamp_ObjectPosition(*draggingObject, canvasSize, draggingObject->position);
                }
            }

            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                _draggingObjectId = 0;

            drawList->AddRect(canvasPos, canvasMax, canvasBoundaryColor, 0.f, 0, 2.f);
            drawList->AddRect(viewportMin, viewportMax, borderColor);
        }

        Set_Open(isOpen);
        ImGui::End();
        ImGui::PopStyleVar();
    }

    Shared<UI_Viewport_View> UI_Viewport_View::Create(UI_Editor_Manager& editorManager)
    {
        return make_shared<UI_Viewport_View>(editorManager);
    }
}
