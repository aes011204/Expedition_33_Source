#include "pch.h"
#include "UI_Viewport_View.h"
#include "UI_Editor_Manager.h"
#include "UI_Editor_Utils.h"
#include "UI_Viewport_Utils.h"

namespace UIEditor
{
    using namespace ViewportUtils;

    namespace
    {
        constexpr float kRotationEpsilon = 0.0001f;

        ImVec2 Rotate_Point(const ImVec2& point, const ImVec2& center, float cosAngle, float sinAngle)
        {
            const ImVec2 offset = ImVec2(point.x - center.x, point.y - center.y);
            return ImVec2(
                center.x + offset.x * cosAngle - offset.y * sinAngle,
                center.y + offset.x * sinAngle + offset.y * cosAngle);
        }

        void Build_RotatedQuad(const ImVec2& minPoint, const ImVec2& maxPoint, float rotationRadians, ImVec2 outQuad[4])
        {
            const ImVec2 center = ImVec2((minPoint.x + maxPoint.x) * 0.5f, (minPoint.y + maxPoint.y) * 0.5f);
            const float cosAngle = cosf(rotationRadians);
            const float sinAngle = sinf(rotationRadians);

            outQuad[0] = Rotate_Point(ImVec2(minPoint.x, minPoint.y), center, cosAngle, sinAngle);
            outQuad[1] = Rotate_Point(ImVec2(maxPoint.x, minPoint.y), center, cosAngle, sinAngle);
            outQuad[2] = Rotate_Point(ImVec2(maxPoint.x, maxPoint.y), center, cosAngle, sinAngle);
            outQuad[3] = Rotate_Point(ImVec2(minPoint.x, maxPoint.y), center, cosAngle, sinAngle);
        }

        void Draw_QuadOutline(ImDrawList* drawList, const ImVec2 quad[4], ImU32 color, float thickness)
        {
            drawList->AddLine(quad[0], quad[1], color, thickness);
            drawList->AddLine(quad[1], quad[2], color, thickness);
            drawList->AddLine(quad[2], quad[3], color, thickness);
            drawList->AddLine(quad[3], quad[0], color, thickness);
        }
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
        if (ImGui::Begin("Viewport", &isOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

            ImGui::SetCursorPosX(8.f);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Zoom: %.0f%%", _zoomFactor * 100.f);
            ImGui::SameLine();
            if (ImGui::Checkbox("Preview", &_previewMode))
                Cancel_InteractionState(true);
            ImGui::SameLine();
            ImGui::BeginDisabled(_previewMode);
            ImGui::Checkbox("Grid", &_showGrid);
            ImGui::SameLine();
            ImGui::Checkbox("Snap", &_snapEnabled);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(82.f);
            ImGui::DragFloat("Grid Size", &_gridSize, 1.f, kMinGridSize, kMaxGridSize, "%.0f");
            ImGui::EndDisabled();
            ImGui::Separator();

            ImGui::BeginChild("##UIEditorViewportScrollRegion", ImVec2(0.f, 0.f), false, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

            const ImVec2 contentCursorPos = ImGui::GetCursorPos();
            ImVec2 scroll = ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());
            const bool hadHorizontalScrollbar = ImGui::GetScrollMaxX() > 0.f;
            const bool hadVerticalScrollbar = ImGui::GetScrollMaxY() > 0.f;
            const float scrollbarSize = ImGui::GetStyle().ScrollbarSize;
            const ImVec2 childWindowPos = ImGui::GetWindowPos();
            const ImVec2 childWindowSize = ImGui::GetWindowSize();
            ImVec2 available = ImVec2(
                max(1.f, childWindowSize.x - (hadVerticalScrollbar ? scrollbarSize : 0.f)),
                max(1.f, childWindowSize.y - (hadHorizontalScrollbar ? scrollbarSize : 0.f)));
            const ImVec2 viewAreaMin = childWindowPos;
            const ImVec2 viewAreaMax = ImVec2(viewAreaMin.x + available.x, viewAreaMin.y + available.y);

            UI_Document& document = _editorManager.Get_Document();
            const ImVec2 canvasSize = document.Get_CanvasSize();
            _gridSize = min(max(_gridSize, kMinGridSize), kMaxGridSize);

            const float canvasWidth = max(1.f, canvasSize.x);
            const float canvasHeight = max(1.f, canvasSize.y);
            const float fitScale = max(0.01f, min(available.x / canvasWidth, available.y / canvasHeight));

            auto calculateCanvasScreenSize = [&](float zoomFactor) -> ImVec2
            {
                const float scale = fitScale * zoomFactor;
                return ImVec2(canvasWidth * scale, canvasHeight * scale);
            };

            auto calculateCanvasOffset = [&](const ImVec2& screenSize) -> ImVec2
            {
                return ImVec2(
                    max(0.f, (available.x - screenSize.x) * 0.5f),
                    max(0.f, (available.y - screenSize.y) * 0.5f));
            };

            auto calculateScrollMax = [&](const ImVec2& screenSize, const ImVec2& offset) -> ImVec2
            {
                const ImVec2 contentSize = ImVec2(
                    contentCursorPos.x + offset.x + screenSize.x,
                    contentCursorPos.y + offset.y + screenSize.y);
                return ImVec2(
                    max(0.f, contentSize.x - available.x),
                    max(0.f, contentSize.y - available.y));
            };

            const ImVec2 mousePos = ImGui::GetMousePos();
            ImGuiIO& io = ImGui::GetIO();
            const bool hasBlockingPopup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);
            const bool viewportHoveredForInput = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
            const bool viewportInputAllowed = !hasBlockingPopup && viewportHoveredForInput;
            if (hasBlockingPopup)
                Cancel_InteractionState(true);

            const bool canZoomViewport = viewportInputAllowed;
            if (canZoomViewport && io.MouseWheel != 0.f)
            {
                const float previousZoomFactor = _zoomFactor;
                const float previousDisplayScale = fitScale * previousZoomFactor;
                const ImVec2 previousCanvasScreenSize = calculateCanvasScreenSize(previousZoomFactor);
                const ImVec2 previousCanvasOffset = calculateCanvasOffset(previousCanvasScreenSize);
                const ImVec2 previousCanvasPos = ImVec2(
                    viewAreaMin.x + previousCanvasOffset.x - scroll.x,
                    viewAreaMin.y + previousCanvasOffset.y - scroll.y);
                const ImVec2 mouseCanvasAnchor = ImVec2(
                    min(max((mousePos.x - previousCanvasPos.x) / previousDisplayScale, 0.f), canvasWidth),
                    min(max((mousePos.y - previousCanvasPos.y) / previousDisplayScale, 0.f), canvasHeight));
                const ImVec2 mouseViewportOffset = ImVec2(mousePos.x - viewAreaMin.x, mousePos.y - viewAreaMin.y);

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

                _zoomFactor = Clamp_Zoom(_zoomFactor);
                if (_zoomFactor <= 1.f)
                {
                    _zoomFactor = 1.f;
                    scroll = ImVec2(0.f, 0.f);
                }
                else
                {
                    const float nextDisplayScale = fitScale * _zoomFactor;
                    const ImVec2 nextCanvasScreenSize = calculateCanvasScreenSize(_zoomFactor);
                    const ImVec2 nextCanvasOffset = calculateCanvasOffset(nextCanvasScreenSize);
                    const ImVec2 nextScrollMax = calculateScrollMax(nextCanvasScreenSize, nextCanvasOffset);
                    scroll = ImVec2(
                        min(max(nextCanvasOffset.x + mouseCanvasAnchor.x * nextDisplayScale - mouseViewportOffset.x, 0.f), nextScrollMax.x),
                        min(max(nextCanvasOffset.y + mouseCanvasAnchor.y * nextDisplayScale - mouseViewportOffset.y, 0.f), nextScrollMax.y));
                }

                ImGui::SetScrollX(scroll.x);
                ImGui::SetScrollY(scroll.y);
            }

            _zoomFactor = Clamp_Zoom(_zoomFactor);
            if (_zoomFactor <= 1.f)
            {
                _zoomFactor = 1.f;
                scroll = ImVec2(0.f, 0.f);
                ImGui::SetScrollX(0.f);
                ImGui::SetScrollY(0.f);
            }

            const float displayScale = fitScale * _zoomFactor;
            const ImVec2 canvasScreenSize = calculateCanvasScreenSize(_zoomFactor);
            const ImVec2 canvasOffset = calculateCanvasOffset(canvasScreenSize);
            const ImVec2 viewportInteractionSize = ImVec2(canvasOffset.x + canvasScreenSize.x, canvasOffset.y + canvasScreenSize.y);
            const ImVec2 scrollMax = calculateScrollMax(canvasScreenSize, canvasOffset);

            const ImVec2 viewportMin = viewAreaMin;
            const ImVec2 viewportMax = viewAreaMax;
            const bool isMouseInViewport =
                mousePos.x >= viewportMin.x && mousePos.x <= viewportMax.x &&
                mousePos.y >= viewportMin.y && mousePos.y <= viewportMax.y;
            const bool hasScrollableViewport = scrollMax.x > 0.f || scrollMax.y > 0.f;

            const bool canPanViewport = !hasBlockingPopup && hasScrollableViewport && ((viewportInputAllowed && isMouseInViewport) || _isPanningViewport);
            if (!_isPanningViewport && canPanViewport && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                _isPanningViewport = true;

            if (_isPanningViewport)
            {
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) || !hasScrollableViewport)
                {
                    _isPanningViewport = false;
                }
                else
                {
                    scroll = ImVec2(
                        min(max(scroll.x - io.MouseDelta.x, 0.f), scrollMax.x),
                        min(max(scroll.y - io.MouseDelta.y, 0.f), scrollMax.y));
                    ImGui::SetScrollX(scroll.x);
                    ImGui::SetScrollY(scroll.y);
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                }
            }

            const ImVec2 canvasPos = ImVec2(viewAreaMin.x + canvasOffset.x - scroll.x, viewAreaMin.y + canvasOffset.y - scroll.y);
            const ImVec2 canvasMax = ImVec2(canvasPos.x + canvasScreenSize.x, canvasPos.y + canvasScreenSize.y);

            const ImU32 viewportColor = IM_COL32(15, 17, 20, 255);
            const ImU32 canvasColor = IM_COL32(26, 28, 32, 255);
            const ImU32 gridColor = IM_COL32(52, 56, 64, 160);
            const ImU32 canvasBoundaryColor = IM_COL32(150, 160, 174, 220);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->PushClipRect(viewportMin, viewportMax, true);

            drawList->AddRectFilled(viewportMin, viewportMax, viewportColor);
            drawList->AddRectFilled(canvasPos, canvasMax, canvasColor);

            if (!_previewMode && _showGrid)
            {
                for (float x = 0.f; x <= canvasSize.x; x += _gridSize)
                {
                    float screenX = canvasPos.x + x * displayScale;
                    drawList->AddLine(ImVec2(screenX, canvasPos.y), ImVec2(screenX, canvasMax.y), gridColor);
                }
                for (float y = 0.f; y <= canvasSize.y; y += _gridSize)
                {
                    float screenY = canvasPos.y + y * displayScale;
                    drawList->AddLine(ImVec2(canvasPos.x, screenY), ImVec2(canvasMax.x, screenY), gridColor);
                }
            }

            ImGui::SetCursorPos(contentCursorPos);
            ImGui::Dummy(viewportInteractionSize);
            const bool isMouseInCanvas =
                mousePos.x >= canvasPos.x && mousePos.x <= canvasMax.x &&
                mousePos.y >= canvasPos.y && mousePos.y <= canvasMax.y;

            const ImVec2 mouseCanvasPos = ImVec2((mousePos.x - canvasPos.x) / displayScale, (mousePos.y - canvasPos.y) / displayScale);
            auto buildViewportClipRect = [&](const ImVec2& objectMin, const ImVec2& objectMax, D3D11_RECT& outRect) -> bool
            {
                const ImVec2 clippedMin = ImVec2(max(objectMin.x, viewportMin.x), max(objectMin.y, viewportMin.y));
                const ImVec2 clippedMax = ImVec2(min(objectMax.x, viewportMax.x), min(objectMax.y, viewportMax.y));
                return Build_ScreenClipRect(clippedMin, clippedMax, outRect);
            };

            const bool isAnimationPreviewActive = _editorManager.Is_AnimationPreviewActive();
            if (isAnimationPreviewActive)
            {
                _draggingObjectId = 0;
                _draggingObjectIds.clear();
                _dragStartObjectWorldPositions.clear();
                _resizingObjectId = 0;
                _activeResizeHandle = UI_ResizeHandle::None;
                _isBoxSelecting = false;
            }

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

            auto findObject = [&objects](ObjectId objectId) -> const UI_Object*
            {
                for (const UI_Object& object : objects)
                {
                    if (object.id == objectId)
                        return &object;
                }

                return nullptr;
            };

            auto getPreviewLocalPosition = [this](const UI_Object& object) -> ImVec2
            {
                UI_AnimationPreviewSample sample = {};
                if (_editorManager.Sample_AnimationPreview(object, sample))
                    return sample.position;

                return object.position;
            };

            function<ImVec2(ObjectId, unordered_set<ObjectId>&)> resolvePreviewWorldPosition =
                [&](ObjectId objectId, unordered_set<ObjectId>& resolvingIds) -> ImVec2
            {
                const UI_Object* object = findObject(objectId);
                if (nullptr == object)
                    return ImVec2(0.f, 0.f);

                if (!resolvingIds.insert(objectId).second)
                    return getPreviewLocalPosition(*object);

                ImVec2 worldPosition = getPreviewLocalPosition(*object);
                if (object->parentId != 0)
                {
                    const ImVec2 parentWorldPosition = resolvePreviewWorldPosition(object->parentId, resolvingIds);
                    worldPosition.x += parentWorldPosition.x;
                    worldPosition.y += parentWorldPosition.y;
                }

                resolvingIds.erase(objectId);
                return worldPosition;
            };

            auto getPreviewWorldPosition = [&resolvePreviewWorldPosition](ObjectId objectId) -> ImVec2
            {
                unordered_set<ObjectId> resolvingIds;
                return resolvePreviewWorldPosition(objectId, resolvingIds);
            };

            bool hasSelectedObjectRect = false;
            ImVec2 selectedObjectMin = ImVec2(0.f, 0.f);
            ImVec2 selectedObjectMax = ImVec2(0.f, 0.f);
            ImVec2 selectedObjectWorldPosition = ImVec2(0.f, 0.f);
            ImVec2 selectedObjectWorldSize = ImVec2(0.f, 0.f);
            bool selectedObjectIsLocked = false;

            for (const UI_Object* objectPtr : sortedObjects)
            {
                const UI_Object& object = *objectPtr;
                if (!_editorManager.Is_ObjectEffectivelyVisible(object.id))
                    continue;

                UI_Object renderObject = object;
                UI_AnimationPreviewSample previewSample = {};
                if (_editorManager.Sample_AnimationPreview(object, previewSample))
                {
                    renderObject.position = previewSample.position;
                    renderObject.scale = previewSample.scale;
                    renderObject.rotationRadians = previewSample.rotationRadians;
                    renderObject.color = previewSample.color;
                }

                const ImVec2 objectWorldPosition = getPreviewWorldPosition(object.id);
                const ImVec2 objectSize = Get_UIObjectRuntimeSize(renderObject);
                const ImVec2 objectMin = ImVec2(canvasPos.x + objectWorldPosition.x * displayScale, canvasPos.y + objectWorldPosition.y * displayScale);
                const ImVec2 objectMax = ImVec2(objectMin.x + objectSize.x * displayScale, objectMin.y + objectSize.y * displayScale);
                const bool isSelected = _editorManager.Is_ObjectSelected(object.id);
                const bool isPrimarySelected = (_editorManager.Get_SelectedObjectId() == object.id);
                const bool isEffectivelyLocked = _editorManager.Is_ObjectEffectivelyLocked(object.id);

                if (isPrimarySelected)
                {
                    hasSelectedObjectRect = true;
                    selectedObjectMin = objectMin;
                    selectedObjectMax = objectMax;
                    selectedObjectWorldPosition = objectWorldPosition;
                    selectedObjectWorldSize = objectSize;
                    selectedObjectIsLocked = isEffectivelyLocked;
                }

                const ImU32 fillColor = Is_ContainerObjectType(renderObject.type) ? ImGui::ColorConvertFloat4ToU32(renderObject.color) : Get_ObjectFillColor(renderObject.type);
                const ImU32 objectBorderColor = isPrimarySelected
                    ? (isEffectivelyLocked ? IM_COL32(255, 167, 38, 255) : IM_COL32(255, 217, 102, 255))
                    : (isSelected ? (isEffectivelyLocked ? IM_COL32(255, 183, 77, 245) : IM_COL32(102, 205, 255, 245)) : IM_COL32(210, 214, 220, 220));
                const float borderThickness = isPrimarySelected ? 2.5f : (isSelected ? 2.f : 1.5f);
                const float objectRounding = renderObject.type == UI_ObjectType::Button ? 4.f : 0.f;
                ID3D11ShaderResourceView* previewSRV = nullptr;

                if (renderObject.type == UI_ObjectType::Image && (!renderObject.textureGuid.empty() || !renderObject.texturePath.empty()))
                {
                    if (!renderObject.maskTextureGuid.empty() || !renderObject.maskTexturePath.empty())
                    {
                        previewSRV = _editorManager.Get_MaskedTexturePreviewSRV(
                            renderObject.textureGuid,
                            renderObject.texturePath,
                            renderObject.maskTextureGuid,
                            renderObject.maskTexturePath);
                    }

                    if (nullptr == previewSRV)
                        previewSRV = _editorManager.Get_TexturePreviewSRV(renderObject.textureGuid, renderObject.texturePath);
                }

                const bool hasTextContent = renderObject.type == UI_ObjectType::Text && !renderObject.text.empty();
                const bool isRotated = fabsf(renderObject.rotationRadians) > kRotationEpsilon;
                ImVec2 rotatedQuad[4] = {};
                if (isRotated)
                    Build_RotatedQuad(objectMin, objectMax, renderObject.rotationRadians, rotatedQuad);

                if (nullptr != previewSRV)
                {
                    if (isRotated)
                    {
                        drawList->AddImageQuad(
                            To_ImTextureID(previewSRV),
                            rotatedQuad[0],
                            rotatedQuad[1],
                            rotatedQuad[2],
                            rotatedQuad[3],
                            ImVec2(0.f, 0.f),
                            ImVec2(1.f, 0.f),
                            ImVec2(1.f, 1.f),
                            ImVec2(0.f, 1.f),
                            ImGui::ColorConvertFloat4ToU32(renderObject.color));
                    }
                    else
                    {
                        drawList->AddImage(
                            To_ImTextureID(previewSRV),
                            objectMin,
                            objectMax,
                            ImVec2(0.f, 0.f),
                            ImVec2(1.f, 1.f),
                            ImGui::ColorConvertFloat4ToU32(renderObject.color));
                    }
                }
                else if (!hasTextContent && (!_previewMode || renderObject.type != UI_ObjectType::Image))
                {
                    if (isRotated)
                        drawList->AddQuadFilled(rotatedQuad[0], rotatedQuad[1], rotatedQuad[2], rotatedQuad[3], fillColor);
                    else
                        drawList->AddRectFilled(objectMin, objectMax, fillColor, objectRounding);
                }

                if (!_previewMode)
                {
                    if (isRotated)
                        Draw_QuadOutline(drawList, rotatedQuad, objectBorderColor, borderThickness);
                    else
                        drawList->AddRect(objectMin, objectMax, objectBorderColor, objectRounding, 0, borderThickness);
                }

                bool usesSpriteFontPreview = false;
                if (!hasBlockingPopup && renderObject.type == UI_ObjectType::Text && (!renderObject.fontGuid.empty() || !renderObject.fontPath.empty()) && !renderObject.text.empty())
                {
                    D3D11_RECT clipRect = {};
                    if (buildViewportClipRect(viewportMin, viewportMax, clipRect))
                    {
                        usesSpriteFontPreview = _editorManager.Queue_FontPreview(
                            renderObject.fontGuid,
                            renderObject.fontPath,
                            String::ToWString(renderObject.text),
                            Vec2(objectMin.x, objectMin.y),
                            Vec2(objectMax.x, objectMax.y),
                            Vec4(renderObject.color.x, renderObject.color.y, renderObject.color.z, renderObject.color.w),
                            renderObject.rotationRadians,
                            renderObject.textAlign,
                            Get_UITextRuntimeScale(renderObject) * displayScale,
                            renderObject.textWrap,
                            clipRect);
                    }
                }

                if (!usesSpriteFontPreview && nullptr == previewSRV && objectMax.x - objectMin.x > 32.f && objectMax.y - objectMin.y > 18.f)
                {
                    if (renderObject.type == UI_ObjectType::Text)
                    {
                        Draw_ImGuiTextPreview(drawList, renderObject, objectMin, objectMax, displayScale);
                    }
                    else if (!_previewMode)
                    {
                        const ImVec2 textPosition = ImVec2(objectMin.x + 4.f, objectMin.y + 3.f);
                        drawList->AddText(textPosition, IM_COL32(245, 247, 250, 230), renderObject.name.c_str());
                    }

                    if (!_previewMode && renderObject.type == UI_ObjectType::Text && isSelected && !renderObject.fontTag.empty() && objectMax.y - objectMin.y > 34.f)
                    {
                        const string fontPreviewText = "Font: " + renderObject.fontTag;
                        drawList->AddText(ImVec2(objectMin.x + 4.f, objectMax.y - 17.f), IM_COL32(255, 217, 102, 240), fontPreviewText.c_str());
                    }
                }

                if (!_previewMode && isEffectivelyLocked && objectMax.x - objectMin.x > 32.f && objectMax.y - objectMin.y > 18.f)
                    drawList->AddText(ImVec2(objectMax.x - 30.f, objectMin.y + 3.f), IM_COL32(255, 217, 102, 245), "LOCK");
            }

            UI_ResizeHandle hoveredResizeHandle = UI_ResizeHandle::None;
            if (!_previewMode && hasSelectedObjectRect && !selectedObjectIsLocked)
            {
                Draw_ResizeHandles(drawList, selectedObjectMin, selectedObjectMax);
                hoveredResizeHandle = Hit_TestResizeHandle(mousePos, selectedObjectMin, selectedObjectMax);
                if (hoveredResizeHandle != UI_ResizeHandle::None)
                    Set_ResizeMouseCursor(hoveredResizeHandle);
            }

            if (!_previewMode && !isAnimationPreviewActive && viewportInputAllowed && isMouseInViewport && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                if (hoveredResizeHandle != UI_ResizeHandle::None && _editorManager.Get_SelectedObjectId() != 0)
                {
                    _editorManager.Begin_EditTransaction();
                    _resizingObjectId = _editorManager.Get_SelectedObjectId();
                    _activeResizeHandle = hoveredResizeHandle;
                    _resizeStartMouseCanvasPos = mouseCanvasPos;
                    _resizeStartObjectWorldPosition = selectedObjectWorldPosition;
                    _resizeStartObjectWorldSize = selectedObjectWorldSize;
                    _draggingObjectId = 0;
                    _draggingObjectIds.clear();
                    _dragStartObjectWorldPositions.clear();
                }
                else if (isMouseInCanvas)
                {
                    ObjectId pickedObjectId = 0;
                    for (auto iter = sortedObjects.rbegin(); iter != sortedObjects.rend(); ++iter)
                    {
                        const UI_Object& object = **iter;
                        if (!_editorManager.Is_ObjectEffectivelyVisible(object.id))
                            continue;

                        const ImVec2 objectWorldPosition = document.Get_WorldPosition(object.id);
                        const ImVec2 objectSize = Get_UIObjectRuntimeSize(object);
                        const bool isInsideObject =
                            mouseCanvasPos.x >= objectWorldPosition.x && mouseCanvasPos.x <= objectWorldPosition.x + objectSize.x &&
                            mouseCanvasPos.y >= objectWorldPosition.y && mouseCanvasPos.y <= objectWorldPosition.y + objectSize.y;

                        if (isInsideObject)
                        {
                            pickedObjectId = object.id;
                            break;
                        }
                    }

                    if (pickedObjectId != 0)
                    {
                        const bool wasSelected = _editorManager.Is_ObjectSelected(pickedObjectId);

                        if (io.KeyCtrl)
                        {
                            _editorManager.Toggle_Selection(pickedObjectId);
                        }
                        else if (io.KeyShift)
                        {
                            _editorManager.Add_ToSelection(pickedObjectId);
                        }
                        else if (wasSelected && _editorManager.Get_SelectedObjectCount() > 1)
                        {
                            _editorManager.Set_SelectedObjectIds(_editorManager.Get_SelectedObjectIds(), pickedObjectId);
                        }
                        else
                        {
                            _editorManager.Set_SingleSelection(pickedObjectId);
                        }

                        if (_editorManager.Is_ObjectSelected(pickedObjectId))
                        {
                            _draggingObjectId = 0;
                            _draggingObjectIds.clear();
                            _dragStartObjectWorldPositions.clear();
                            _dragStartMouseCanvasPos = mouseCanvasPos;

                            if (!_editorManager.Is_ObjectEffectivelyLocked(pickedObjectId))
                            {
                                _draggingObjectId = pickedObjectId;
                                const vector<ObjectId> rootSelectedObjectIds = _editorManager.Get_RootSelectedObjectIds();
                                for (ObjectId draggingObjectId : rootSelectedObjectIds)
                                {
                                    if (_editorManager.Is_ObjectEffectivelyLocked(draggingObjectId))
                                        continue;

                                    _draggingObjectIds.push_back(draggingObjectId);
                                    _dragStartObjectWorldPositions[draggingObjectId] = document.Get_WorldPosition(draggingObjectId);
                                }

                                if (!_draggingObjectIds.empty())
                                {
                                    _dragStartObjectWorldPosition = document.Get_WorldPosition(pickedObjectId);
                                    _editorManager.Begin_EditTransaction();
                                }
                            }
                        }
                    }
                    else
                    {
                        if (!io.KeyCtrl && !io.KeyShift)
                            _editorManager.Clear_Selection();

                        _isBoxSelecting = true;
                        _boxSelectStartCanvasPos = mouseCanvasPos;
                        _boxSelectCurrentCanvasPos = mouseCanvasPos;
                        _draggingObjectId = 0;
                        _draggingObjectIds.clear();
                        _dragStartObjectWorldPositions.clear();
                    }
                }
            }

            if (!_previewMode && !hasBlockingPopup && !isAnimationPreviewActive && _resizingObjectId != 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                UI_Object* resizingObject = document.Find_Object(_resizingObjectId);
                if (nullptr != resizingObject)
                {
                    const bool movesLeft =
                        _activeResizeHandle == UI_ResizeHandle::West ||
                        _activeResizeHandle == UI_ResizeHandle::NorthWest ||
                        _activeResizeHandle == UI_ResizeHandle::SouthWest;
                    const bool movesRight =
                        _activeResizeHandle == UI_ResizeHandle::East ||
                        _activeResizeHandle == UI_ResizeHandle::NorthEast ||
                        _activeResizeHandle == UI_ResizeHandle::SouthEast;
                    const bool movesTop =
                        _activeResizeHandle == UI_ResizeHandle::North ||
                        _activeResizeHandle == UI_ResizeHandle::NorthEast ||
                        _activeResizeHandle == UI_ResizeHandle::NorthWest;
                    const bool movesBottom =
                        _activeResizeHandle == UI_ResizeHandle::South ||
                        _activeResizeHandle == UI_ResizeHandle::SouthEast ||
                        _activeResizeHandle == UI_ResizeHandle::SouthWest;

                    const ImVec2 delta = ImVec2(
                        mouseCanvasPos.x - _resizeStartMouseCanvasPos.x,
                        mouseCanvasPos.y - _resizeStartMouseCanvasPos.y);

                    ImVec2 nextMin = _resizeStartObjectWorldPosition;
                    ImVec2 nextMax = ImVec2(
                        _resizeStartObjectWorldPosition.x + _resizeStartObjectWorldSize.x,
                        _resizeStartObjectWorldPosition.y + _resizeStartObjectWorldSize.y);

                    if (movesLeft)
                        nextMin.x += delta.x;
                    if (movesRight)
                        nextMax.x += delta.x;
                    if (movesTop)
                        nextMin.y += delta.y;
                    if (movesBottom)
                        nextMax.y += delta.y;

                    if (_snapEnabled)
                    {
                        if (movesLeft)
                            nextMin.x = Snap_Value(nextMin.x, _gridSize);
                        if (movesRight)
                            nextMax.x = Snap_Value(nextMax.x, _gridSize);
                        if (movesTop)
                            nextMin.y = Snap_Value(nextMin.y, _gridSize);
                        if (movesBottom)
                            nextMax.y = Snap_Value(nextMax.y, _gridSize);
                    }

                    if (nextMax.x - nextMin.x < 1.f)
                    {
                        if (movesLeft)
                            nextMin.x = nextMax.x - 1.f;
                        else
                            nextMax.x = nextMin.x + 1.f;
                    }

                    if (nextMax.y - nextMin.y < 1.f)
                    {
                        if (movesTop)
                            nextMin.y = nextMax.y - 1.f;
                        else
                            nextMax.y = nextMin.y + 1.f;
                    }

                    nextMin.x = min(max(nextMin.x, 0.f), canvasSize.x - 1.f);
                    nextMin.y = min(max(nextMin.y, 0.f), canvasSize.y - 1.f);
                    nextMax.x = min(max(nextMax.x, nextMin.x + 1.f), canvasSize.x);
                    nextMax.y = min(max(nextMax.y, nextMin.y + 1.f), canvasSize.y);

                    _editorManager.Set_UIObjectWorldRect(
                        _resizingObjectId,
                        nextMin,
                        ImVec2(nextMax.x - nextMin.x, nextMax.y - nextMin.y));
                }
            }
            else if (!_previewMode && !hasBlockingPopup && !isAnimationPreviewActive && _draggingObjectId != 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.f))
            {
                const ImVec2 delta = ImVec2(
                    mouseCanvasPos.x - _dragStartMouseCanvasPos.x,
                    mouseCanvasPos.y - _dragStartMouseCanvasPos.y);

                for (ObjectId draggingObjectId : _draggingObjectIds)
                {
                    UI_Object* draggingObject = document.Find_Object(draggingObjectId);
                    if (nullptr == draggingObject)
                        continue;

                    const auto startPositionIter = _dragStartObjectWorldPositions.find(draggingObjectId);
                    if (startPositionIter == _dragStartObjectWorldPositions.end())
                        continue;

                    const ImVec2 startWorldPosition = startPositionIter->second;
                    ImVec2 nextWorldPosition = ImVec2(
                        startWorldPosition.x + delta.x,
                        startWorldPosition.y + delta.y);

                    if (_snapEnabled)
                        nextWorldPosition = Snap_Position(nextWorldPosition, _gridSize);

                    const ImVec2 clampedWorldPosition = Clamp_ObjectPosition(*draggingObject, canvasSize, nextWorldPosition);
                    _editorManager.Set_UIObjectWorldPosition(draggingObjectId, clampedWorldPosition);
                }
            }

            if (!_previewMode && !hasBlockingPopup && !isAnimationPreviewActive && _isBoxSelecting && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                _boxSelectCurrentCanvasPos = ImVec2(
                    min(max(mouseCanvasPos.x, 0.f), canvasSize.x),
                    min(max(mouseCanvasPos.y, 0.f), canvasSize.y));

                const ImVec2 boxCanvasMin = ImVec2(
                    min(_boxSelectStartCanvasPos.x, _boxSelectCurrentCanvasPos.x),
                    min(_boxSelectStartCanvasPos.y, _boxSelectCurrentCanvasPos.y));
                const ImVec2 boxCanvasMax = ImVec2(
                    max(_boxSelectStartCanvasPos.x, _boxSelectCurrentCanvasPos.x),
                    max(_boxSelectStartCanvasPos.y, _boxSelectCurrentCanvasPos.y));
                const ImVec2 boxScreenMin = ImVec2(canvasPos.x + boxCanvasMin.x * displayScale, canvasPos.y + boxCanvasMin.y * displayScale);
                const ImVec2 boxScreenMax = ImVec2(canvasPos.x + boxCanvasMax.x * displayScale, canvasPos.y + boxCanvasMax.y * displayScale);

                drawList->AddRectFilled(boxScreenMin, boxScreenMax, IM_COL32(102, 205, 255, 34));
                drawList->AddRect(boxScreenMin, boxScreenMax, IM_COL32(102, 205, 255, 220), 0.f, 0, 1.5f);
            }

            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                if (_isBoxSelecting)
                {
                    const ImVec2 boxCanvasMin = ImVec2(
                        min(_boxSelectStartCanvasPos.x, _boxSelectCurrentCanvasPos.x),
                        min(_boxSelectStartCanvasPos.y, _boxSelectCurrentCanvasPos.y));
                    const ImVec2 boxCanvasMax = ImVec2(
                        max(_boxSelectStartCanvasPos.x, _boxSelectCurrentCanvasPos.x),
                        max(_boxSelectStartCanvasPos.y, _boxSelectCurrentCanvasPos.y));
                    const float boxWidth = boxCanvasMax.x - boxCanvasMin.x;
                    const float boxHeight = boxCanvasMax.y - boxCanvasMin.y;

                    if (boxWidth > 1.f || boxHeight > 1.f)
                    {
                        for (const UI_Object* objectPtr : sortedObjects)
                        {
                            const UI_Object& object = *objectPtr;
                            if (!_editorManager.Is_ObjectEffectivelyVisible(object.id))
                                continue;

                            const ImVec2 objectWorldPosition = document.Get_WorldPosition(object.id);
                            const ImVec2 objectSize = Get_UIObjectRuntimeSize(object);
                            const ImVec2 objectMax = ImVec2(objectWorldPosition.x + objectSize.x, objectWorldPosition.y + objectSize.y);
                            const bool intersects =
                                objectWorldPosition.x <= boxCanvasMax.x &&
                                objectMax.x >= boxCanvasMin.x &&
                                objectWorldPosition.y <= boxCanvasMax.y &&
                                objectMax.y >= boxCanvasMin.y;

                            if (intersects)
                                _editorManager.Add_ToSelection(object.id);
                        }
                    }

                    _isBoxSelecting = false;
                }

                if (_draggingObjectId != 0 || _resizingObjectId != 0)
                    _editorManager.Commit_EditTransactionIfChanged();

                _draggingObjectId = 0;
                _draggingObjectIds.clear();
                _dragStartObjectWorldPositions.clear();
                _resizingObjectId = 0;
                _activeResizeHandle = UI_ResizeHandle::None;
            }

            if (!_previewMode)
                drawList->AddRect(canvasPos, canvasMax, canvasBoundaryColor, 0.f, 0, 2.f);
            drawList->PopClipRect();
            ImGui::EndChild();
        }

        Set_Open(isOpen);
        ImGui::End();
        ImGui::PopStyleVar();
    }

    Shared<UI_Viewport_View> UI_Viewport_View::Create(UI_Editor_Manager& editorManager)
    {
        return make_shared<UI_Viewport_View>(editorManager);
    }

    void UI_Viewport_View::Cancel_InteractionState(bool commitTransaction)
    {
        if (commitTransaction && (_draggingObjectId != 0 || _resizingObjectId != 0))
            _editorManager.Commit_EditTransactionIfChanged();

        _isPanningViewport = false;
        _draggingObjectId = 0;
        _draggingObjectIds.clear();
        _dragStartObjectWorldPositions.clear();
        _resizingObjectId = 0;
        _activeResizeHandle = UI_ResizeHandle::None;
        _isBoxSelecting = false;
    }
}
