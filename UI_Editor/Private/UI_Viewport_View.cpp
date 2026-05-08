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
        constexpr float kMinGridSize = 2.f;
        constexpr float kMaxGridSize = 256.f;

        float Clamp_Zoom(float zoomFactor)
        {
            return min(max(zoomFactor, kMinViewportZoomFactor), kMaxViewportZoomFactor);
        }

        ImVec2 Snap_Position(const ImVec2& position, float gridSize)
        {
            const float safeGridSize = max(kMinGridSize, gridSize);
            return ImVec2(
                roundf(position.x / safeGridSize) * safeGridSize,
                roundf(position.y / safeGridSize) * safeGridSize);
        }

        float Snap_Value(float value, float gridSize)
        {
            const float safeGridSize = max(kMinGridSize, gridSize);
            return roundf(value / safeGridSize) * safeGridSize;
        }

        ImVec2 Get_ResizeHandleCenter(UI_ResizeHandle resizeHandle, const ImVec2& objectMin, const ImVec2& objectMax)
        {
            const float centerX = (objectMin.x + objectMax.x) * 0.5f;
            const float centerY = (objectMin.y + objectMax.y) * 0.5f;

            switch (resizeHandle)
            {
            case UI_ResizeHandle::North:
                return ImVec2(centerX, objectMin.y);
            case UI_ResizeHandle::NorthEast:
                return ImVec2(objectMax.x, objectMin.y);
            case UI_ResizeHandle::East:
                return ImVec2(objectMax.x, centerY);
            case UI_ResizeHandle::SouthEast:
                return ImVec2(objectMax.x, objectMax.y);
            case UI_ResizeHandle::South:
                return ImVec2(centerX, objectMax.y);
            case UI_ResizeHandle::SouthWest:
                return ImVec2(objectMin.x, objectMax.y);
            case UI_ResizeHandle::West:
                return ImVec2(objectMin.x, centerY);
            case UI_ResizeHandle::NorthWest:
                return ImVec2(objectMin.x, objectMin.y);
            case UI_ResizeHandle::None:
            default:
                return ImVec2(0.f, 0.f);
            }
        }

        UI_ResizeHandle Hit_TestResizeHandle(const ImVec2& mousePosition, const ImVec2& objectMin, const ImVec2& objectMax)
        {
            constexpr float hitSize = 10.f;
            const UI_ResizeHandle handles[] = {
                UI_ResizeHandle::NorthWest,
                UI_ResizeHandle::NorthEast,
                UI_ResizeHandle::SouthEast,
                UI_ResizeHandle::SouthWest,
                UI_ResizeHandle::North,
                UI_ResizeHandle::East,
                UI_ResizeHandle::South,
                UI_ResizeHandle::West,
            };

            for (UI_ResizeHandle handle : handles)
            {
                const ImVec2 center = Get_ResizeHandleCenter(handle, objectMin, objectMax);
                if (mousePosition.x >= center.x - hitSize * 0.5f && mousePosition.x <= center.x + hitSize * 0.5f &&
                    mousePosition.y >= center.y - hitSize * 0.5f && mousePosition.y <= center.y + hitSize * 0.5f)
                {
                    return handle;
                }
            }

            return UI_ResizeHandle::None;
        }

        void Draw_ResizeHandles(ImDrawList* drawList, const ImVec2& objectMin, const ImVec2& objectMax)
        {
            if (nullptr == drawList)
                return;

            constexpr float handleSize = 8.f;
            const UI_ResizeHandle handles[] = {
                UI_ResizeHandle::North,
                UI_ResizeHandle::NorthEast,
                UI_ResizeHandle::East,
                UI_ResizeHandle::SouthEast,
                UI_ResizeHandle::South,
                UI_ResizeHandle::SouthWest,
                UI_ResizeHandle::West,
                UI_ResizeHandle::NorthWest,
            };

            for (UI_ResizeHandle handle : handles)
            {
                const ImVec2 center = Get_ResizeHandleCenter(handle, objectMin, objectMax);
                const ImVec2 handleMin = ImVec2(center.x - handleSize * 0.5f, center.y - handleSize * 0.5f);
                const ImVec2 handleMax = ImVec2(center.x + handleSize * 0.5f, center.y + handleSize * 0.5f);
                drawList->AddRectFilled(handleMin, handleMax, IM_COL32(255, 217, 102, 255));
                drawList->AddRect(handleMin, handleMax, IM_COL32(36, 39, 46, 255));
            }
        }

        void Set_ResizeMouseCursor(UI_ResizeHandle resizeHandle)
        {
            switch (resizeHandle)
            {
            case UI_ResizeHandle::North:
            case UI_ResizeHandle::South:
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                break;
            case UI_ResizeHandle::East:
            case UI_ResizeHandle::West:
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                break;
            case UI_ResizeHandle::NorthEast:
            case UI_ResizeHandle::SouthWest:
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
                break;
            case UI_ResizeHandle::SouthEast:
            case UI_ResizeHandle::NorthWest:
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
                break;
            case UI_ResizeHandle::None:
            default:
                break;
            }
        }

        vector<string> Split_TextLines(const string& text)
        {
            vector<string> lines;
            size_t lineStart = 0;

            for (size_t i = 0; i <= text.size(); ++i)
            {
                if (i == text.size() || text[i] == '\n')
                {
                    size_t lineEnd = i;
                    if (lineEnd > lineStart && text[lineEnd - 1] == '\r')
                        --lineEnd;

                    lines.push_back(text.substr(lineStart, lineEnd - lineStart));
                    lineStart = i + 1;
                }
            }

            if (lines.empty())
                lines.push_back("");

            return lines;
        }

        vector<string> Wrap_Paragraph(ImFont* font, const string& paragraph, float fontSize, float maxWidth)
        {
            vector<string> wrappedLines;
            if (nullptr == font || paragraph.empty() || maxWidth <= 1.f)
            {
                wrappedLines.push_back(paragraph);
                return wrappedLines;
            }

            string currentLine;
            size_t wordStart = 0;

            auto pushWord = [&](const string& word)
            {
                if (word.empty())
                    return;

                const string candidate = currentLine.empty() ? word : currentLine + " " + word;
                const float candidateWidth = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, candidate.c_str()).x;
                if (candidateWidth <= maxWidth || currentLine.empty())
                {
                    currentLine = candidate;
                    return;
                }

                wrappedLines.push_back(currentLine);
                currentLine = word;
            };

            for (size_t i = 0; i <= paragraph.size(); ++i)
            {
                if (i == paragraph.size() || paragraph[i] == ' ' || paragraph[i] == '\t')
                {
                    pushWord(paragraph.substr(wordStart, i - wordStart));
                    wordStart = i + 1;
                }
            }

            if (!currentLine.empty())
                wrappedLines.push_back(currentLine);

            if (wrappedLines.empty())
                wrappedLines.push_back("");

            return wrappedLines;
        }

        vector<string> Build_TextPreviewLines(ImFont* font, const string& text, float fontSize, float maxWidth, bool wrapText)
        {
            vector<string> lines;
            const vector<string> rawLines = Split_TextLines(text);
            for (const string& rawLine : rawLines)
            {
                if (!wrapText)
                {
                    lines.push_back(rawLine);
                    continue;
                }

                vector<string> wrappedLines = Wrap_Paragraph(font, rawLine, fontSize, maxWidth);
                lines.insert(lines.end(), wrappedLines.begin(), wrappedLines.end());
            }

            if (lines.empty())
                lines.push_back("");

            return lines;
        }

        float Resolve_TextLineX(const ImVec2& objectMin, const ImVec2& objectMax, float lineWidth, UI_TextAlign textAlign)
        {
            constexpr float textPaddingX = 4.f;
            const float textWidth = max(0.f, objectMax.x - objectMin.x);

            switch (textAlign)
            {
            case UI_TextAlign::Center:
                return objectMin.x + max(textPaddingX, (textWidth - lineWidth) * 0.5f);
            case UI_TextAlign::Right:
                return objectMax.x - lineWidth - textPaddingX;
            case UI_TextAlign::Left:
            default:
                return objectMin.x + textPaddingX;
            }
        }

        void Draw_ImGuiTextPreview(
            ImDrawList* drawList,
            const UI_Object& object,
            const ImVec2& objectMin,
            const ImVec2& objectMax,
            float displayScaleX)
        {
            if (nullptr == drawList || object.text.empty())
                return;

            ImFont* font = ImGui::GetFont();
            if (nullptr == font)
                return;

            const float fontPixelSize = max(4.f, object.fontSize) * max(0.01f, object.scale.x) * displayScaleX;
            const float maxTextWidth = max(1.f, objectMax.x - objectMin.x - 8.f);
            const float lineHeight = max(4.f, fontPixelSize * 1.2f);
            const ImU32 previewTextColor = ImGui::ColorConvertFloat4ToU32(object.color);
            const ImVec4 clipRect = ImVec4(objectMin.x, objectMin.y, objectMax.x, objectMax.y);
            const vector<string> lines = Build_TextPreviewLines(font, object.text, fontPixelSize, maxTextWidth, object.textWrap);

            float textY = objectMin.y + 3.f;
            for (const string& line : lines)
            {
                if (textY >= objectMax.y)
                    break;

                const float lineWidth = font->CalcTextSizeA(fontPixelSize, FLT_MAX, 0.f, line.c_str()).x;
                const float textX = Resolve_TextLineX(objectMin, objectMax, lineWidth, object.textAlign);
                drawList->AddText(font, fontPixelSize, ImVec2(textX, textY), previewTextColor, line.c_str(), nullptr, 0.f, &clipRect);
                textY += lineHeight;
            }
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
        if (ImGui::Begin("Viewport", &isOpen, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

            bool requestPixelZoom = false;

            ImGui::SetCursorPosX(8.f);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Zoom: %.0f%%", _zoomFactor * 100.f);
            ImGui::SameLine();
            if (ImGui::SmallButton("100%"))
                requestPixelZoom = true;
            ImGui::SameLine();
            ImGui::Checkbox("Grid", &_showGrid);
            ImGui::SameLine();
            ImGui::Checkbox("Snap", &_snapEnabled);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(82.f);
            ImGui::DragFloat("Grid Size", &_gridSize, 1.f, kMinGridSize, kMaxGridSize, "%.0f");
            ImGui::Separator();

            ImVec2 available = ImGui::GetContentRegionAvail();
            available.x = max(1.f, available.x);
            available.y = max(1.f, available.y);

            UI_Document& document = _editorManager.Get_Document();
            const ImVec2 canvasSize = document.Get_CanvasSize();
            _gridSize = min(max(_gridSize, kMinGridSize), kMaxGridSize);

            if (requestPixelZoom)
            {
                _zoomFactor = 1.f;
                ImGui::SetScrollX(0.f);
                ImGui::SetScrollY(0.f);
            }

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
            _zoomFactor = Clamp_Zoom(_zoomFactor);
            if (_zoomFactor <= 1.f)
            {
                _zoomFactor = 1.f;
                ImGui::SetScrollX(0.f);
                ImGui::SetScrollY(0.f);
            }

            const float canvasWidth = max(1.f, canvasSize.x);
            const float canvasHeight = max(1.f, canvasSize.y);
            const ImVec2 canvasScreenSize = ImVec2(canvasWidth * _zoomFactor, canvasHeight * _zoomFactor);
            const ImVec2 viewportInteractionSize = _zoomFactor > 1.f
                ? canvasScreenSize
                : ImVec2(max(1.f, available.x), max(1.f, available.y));
            const float displayScaleX = _zoomFactor;
            const float displayScaleY = _zoomFactor;

            const ImVec2 contentCursorPos = ImGui::GetCursorPos();
            const ImVec2 viewAreaMin = ImGui::GetCursorScreenPos();
            const ImVec2 viewAreaMax = ImVec2(viewAreaMin.x + available.x, viewAreaMin.y + available.y);
            const ImVec2 canvasOffset = ImVec2(
                max(0.f, (available.x - canvasScreenSize.x) * 0.5f),
                max(0.f, (available.y - canvasScreenSize.y) * 0.5f));

            ImGui::SetCursorScreenPos(viewAreaMin);
            ImGui::InvisibleButton("##UIEditorViewportPanArea", available, ImGuiButtonFlags_MouseButtonRight);
            const bool isPanAreaHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

            ImGui::SetCursorPos(ImVec2(contentCursorPos.x + canvasOffset.x, contentCursorPos.y + canvasOffset.y));
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

            if (_showGrid)
            {
                for (float x = 0.f; x <= canvasSize.x; x += _gridSize)
                {
                    float screenX = canvasPos.x + x * displayScaleX;
                    drawList->AddLine(ImVec2(screenX, canvasPos.y), ImVec2(screenX, canvasMax.y), gridColor);
                }
                for (float y = 0.f; y <= canvasSize.y; y += _gridSize)
                {
                    float screenY = canvasPos.y + y * displayScaleY;
                    drawList->AddLine(ImVec2(canvasPos.x, screenY), ImVec2(canvasMax.x, screenY), gridColor);
                }
            }

            ImGui::InvisibleButton("##UIEditorViewportCanvas", viewportInteractionSize, ImGuiButtonFlags_MouseButtonLeft);
            const bool isViewportHovered = ImGui::IsItemHovered();
            const bool isViewportActive = ImGui::IsItemActive();
            const ImVec2 mousePos = ImGui::GetMousePos();
            const bool isMouseInViewport =
                mousePos.x >= viewportMin.x && mousePos.x <= viewportMax.x &&
                mousePos.y >= viewportMin.y && mousePos.y <= viewportMax.y;
            const bool hasScrollableViewport = ImGui::GetScrollMaxX() > 0.f || ImGui::GetScrollMaxY() > 0.f;

            const bool canPanViewport = hasScrollableViewport && (isPanAreaHovered || isMouseInViewport || _isPanningViewport);
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
                    const float nextScrollX = min(max(ImGui::GetScrollX() - io.MouseDelta.x, 0.f), ImGui::GetScrollMaxX());
                    const float nextScrollY = min(max(ImGui::GetScrollY() - io.MouseDelta.y, 0.f), ImGui::GetScrollMaxY());
                    ImGui::SetScrollX(nextScrollX);
                    ImGui::SetScrollY(nextScrollY);
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                }
            }

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

            bool hasSelectedObjectRect = false;
            ImVec2 selectedObjectMin = ImVec2(0.f, 0.f);
            ImVec2 selectedObjectMax = ImVec2(0.f, 0.f);
            ImVec2 selectedObjectWorldPosition = ImVec2(0.f, 0.f);
            ImVec2 selectedObjectWorldSize = ImVec2(0.f, 0.f);

            for (const UI_Object* objectPtr : sortedObjects)
            {
                const UI_Object& object = *objectPtr;
                const ImVec2 objectWorldPosition = document.Get_WorldPosition(object.id);
                const ImVec2 objectSize = ImVec2(object.size.x * object.scale.x, object.size.y * object.scale.y);
                const ImVec2 objectMin = ImVec2(canvasPos.x + objectWorldPosition.x * displayScaleX, canvasPos.y + objectWorldPosition.y * displayScaleY);
                const ImVec2 objectMax = ImVec2(objectMin.x + objectSize.x * displayScaleX, objectMin.y + objectSize.y * displayScaleY);
                const bool isSelected = (_editorManager.Get_SelectedObjectId() == object.id);

                if (isSelected)
                {
                    hasSelectedObjectRect = true;
                    selectedObjectMin = objectMin;
                    selectedObjectMax = objectMax;
                    selectedObjectWorldPosition = objectWorldPosition;
                    selectedObjectWorldSize = objectSize;
                }

                const ImU32 fillColor = Is_ContainerObjectType(object.type) ? ImGui::ColorConvertFloat4ToU32(object.color) : Get_ObjectFillColor(object.type);
                const ImU32 objectBorderColor = isSelected ? IM_COL32(255, 217, 102, 255) : IM_COL32(210, 214, 220, 220);
                const float borderThickness = isSelected ? 2.5f : 1.5f;
                const float objectRounding = object.type == UI_ObjectType::Button ? 4.f : 0.f;
                ID3D11ShaderResourceView* previewSRV = nullptr;

                if (object.type == UI_ObjectType::Image && !object.textureGuid.empty())
                {
                    if (!object.maskTextureGuid.empty())
                        previewSRV = _editorManager.Get_MaskedTexturePreviewSRV(object.textureGuid, object.maskTextureGuid);

                    if (nullptr == previewSRV)
                        previewSRV = _editorManager.Get_TexturePreviewSRV(object.textureGuid);
                }

                const bool hasTextContent = object.type == UI_ObjectType::Text && !object.text.empty();

                if (nullptr != previewSRV)
                    drawList->AddImage(To_ImTextureID(previewSRV), objectMin, objectMax);
                else if (!hasTextContent)
                    drawList->AddRectFilled(objectMin, objectMax, fillColor, objectRounding);

                drawList->AddRect(objectMin, objectMax, objectBorderColor, objectRounding, 0, borderThickness);

                bool usesSpriteFontPreview = false;
                const bool canUseSpriteFontPreview = object.textAlign == UI_TextAlign::Left && !object.textWrap;
                if (object.type == UI_ObjectType::Text && canUseSpriteFontPreview && (!object.fontGuid.empty() || !object.fontPath.empty()) && !object.text.empty())
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
                    if (object.type == UI_ObjectType::Text)
                    {
                        Draw_ImGuiTextPreview(drawList, object, objectMin, objectMax, displayScaleX);
                    }
                    else
                    {
                        const ImVec2 textPosition = ImVec2(objectMin.x + 4.f, objectMin.y + 3.f);
                        drawList->AddText(textPosition, IM_COL32(245, 247, 250, 230), object.name.c_str());
                    }

                    if (object.type == UI_ObjectType::Text && isSelected && !object.fontTag.empty() && objectMax.y - objectMin.y > 34.f)
                    {
                        const string fontPreviewText = "Font: " + object.fontTag;
                        drawList->AddText(ImVec2(objectMin.x + 4.f, objectMax.y - 17.f), IM_COL32(255, 217, 102, 240), fontPreviewText.c_str());
                    }
                }
            }

            UI_ResizeHandle hoveredResizeHandle = UI_ResizeHandle::None;
            if (hasSelectedObjectRect)
            {
                Draw_ResizeHandles(drawList, selectedObjectMin, selectedObjectMax);
                hoveredResizeHandle = Hit_TestResizeHandle(mousePos, selectedObjectMin, selectedObjectMax);
                if (hoveredResizeHandle != UI_ResizeHandle::None)
                    Set_ResizeMouseCursor(hoveredResizeHandle);
            }

            if (isViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
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
                }
                else
                {
                    ObjectId pickedObjectId = 0;
                    for (auto iter = sortedObjects.rbegin(); iter != sortedObjects.rend(); ++iter)
                    {
                        const UI_Object& object = **iter;
                        const ImVec2 objectWorldPosition = document.Get_WorldPosition(object.id);
                        const ImVec2 objectSize = ImVec2(object.size.x * object.scale.x, object.size.y * object.scale.y);
                        const bool isInsideObject =
                            mouseCanvasPos.x >= objectWorldPosition.x && mouseCanvasPos.x <= objectWorldPosition.x + objectSize.x &&
                            mouseCanvasPos.y >= objectWorldPosition.y && mouseCanvasPos.y <= objectWorldPosition.y + objectSize.y;

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
                    {
                        _editorManager.Begin_EditTransaction();
                        _dragStartObjectWorldPosition = document.Get_WorldPosition(pickedObjectId);
                    }
                }
            }

            if (_resizingObjectId != 0 && isViewportActive && ImGui::IsMouseDown(ImGuiMouseButton_Left))
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
            else if (_draggingObjectId != 0 && isViewportActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.f))
            {
                UI_Object* draggingObject = document.Find_Object(_draggingObjectId);
                if (nullptr != draggingObject)
                {
                    const ImVec2 delta = ImVec2(
                        mouseCanvasPos.x - _dragStartMouseCanvasPos.x,
                        mouseCanvasPos.y - _dragStartMouseCanvasPos.y);

                    ImVec2 nextWorldPosition = ImVec2(
                        _dragStartObjectWorldPosition.x + delta.x,
                        _dragStartObjectWorldPosition.y + delta.y);

                    if (_snapEnabled)
                        nextWorldPosition = Snap_Position(nextWorldPosition, _gridSize);

                    const ImVec2 clampedWorldPosition = Clamp_ObjectPosition(*draggingObject, canvasSize, nextWorldPosition);
                    _editorManager.Set_UIObjectWorldPosition(_draggingObjectId, clampedWorldPosition);
                }
            }

            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                if (_draggingObjectId != 0 || _resizingObjectId != 0)
                    _editorManager.Commit_EditTransactionIfChanged();

                _draggingObjectId = 0;
                _resizingObjectId = 0;
                _activeResizeHandle = UI_ResizeHandle::None;
            }

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
