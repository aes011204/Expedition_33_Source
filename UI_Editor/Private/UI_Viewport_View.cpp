#include "pch.h"
#include "UI_Viewport_View.h"
#include "UI_Editor_Manager.h"
#include "UI_Editor_Utils.h"
#include "UI_PreviewTransformResolver.h"
#include "UI_Viewport_Utils.h"
#include "UI_SpriteVfx.h"

namespace UIEditor
{
    using namespace ViewportUtils;

    namespace
    {
        constexpr float kRotationEpsilon = 0.0001f;

        void Draw_QuadOutline(ImDrawList* drawList, const ImVec2 quad[4], ImU32 color, float thickness)
        {
            drawList->AddLine(quad[0], quad[1], color, thickness);
            drawList->AddLine(quad[1], quad[2], color, thickness);
            drawList->AddLine(quad[2], quad[3], color, thickness);
            drawList->AddLine(quad[3], quad[0], color, thickness);
        }

        void Draw_AnchorMarker(ImDrawList* drawList, const ImVec2& center, ImU32 fillColor, ImU32 outlineColor, float radius)
        {
            drawList->AddCircleFilled(center, radius, fillColor, 12);
            drawList->AddCircle(center, radius, outlineColor, 12, 1.25f);
            drawList->AddLine(ImVec2(center.x - radius - 2.f, center.y), ImVec2(center.x + radius + 2.f, center.y), outlineColor, 1.f);
            drawList->AddLine(ImVec2(center.x, center.y - radius - 2.f), ImVec2(center.x, center.y + radius + 2.f), outlineColor, 1.f);
        }

        ImVec2 Canvas_ToScreen(const ImVec2& canvasPos, float displayScale, const ImVec2& worldPoint)
        {
            return ImVec2(
                canvasPos.x + worldPoint.x * displayScale,
                canvasPos.y + worldPoint.y * displayScale);
        }

        void Draw_RotatedImageOrQuad(
            ImDrawList* drawList,
            ImTextureID textureId,
            const ImVec2& center,
            const ImVec2& size,
            float rotationRadians,
            ImU32 color)
        {
            const float cosAngle = cosf(rotationRadians);
            const float sinAngle = sinf(rotationRadians);
            const ImVec2 halfSize(size.x * 0.5f, size.y * 0.5f);
            const ImVec2 corners[4] = {
                ImVec2(-halfSize.x, -halfSize.y),
                ImVec2(halfSize.x, -halfSize.y),
                ImVec2(halfSize.x, halfSize.y),
                ImVec2(-halfSize.x, halfSize.y),
            };

            ImVec2 points[4] = {};
            for (int32 index = 0; index < 4; ++index)
            {
                points[index] = ImVec2(
                    center.x + corners[index].x * cosAngle - corners[index].y * sinAngle,
                    center.y + corners[index].x * sinAngle + corners[index].y * cosAngle);
            }

            if (textureId != ImTextureID{})
            {
                drawList->AddImageQuad(
                    textureId,
                    points[0],
                    points[1],
                    points[2],
                    points[3],
                    ImVec2(0.f, 0.f),
                    ImVec2(1.f, 0.f),
                    ImVec2(1.f, 1.f),
                    ImVec2(0.f, 1.f),
                    color);
            }
            else
            {
                drawList->AddQuadFilled(points[0], points[1], points[2], points[3], color);
            }
        }

        string Make_NormalizedPathKey(const wstring& path)
        {
            wstring normalizedPath = path;
            replace(normalizedPath.begin(), normalizedPath.end(), L'\\', L'/');
            transform(normalizedPath.begin(), normalizedPath.end(), normalizedPath.begin(), [](wchar_t character)
                {
                    return static_cast<wchar_t>(towlower(character));
                });

            return String::ToString(normalizedPath);
        }

        bool Try_LoadUIVfxAsset(const string& vfxPath, Client::UISpriteVfxAsset& outAsset)
        {
            struct CachedAsset
            {
                Client::UISpriteVfxAsset asset{};
                fs::file_time_type writeTime{};
                bool loaded{ false };
            };

            static unordered_map<string, CachedAsset> cache{};

            if (vfxPath.empty())
                return false;

            const wstring resolvedPath = Resolve_ClientRuntimeResourcePath(vfxPath);
            if (resolvedPath.empty() || !fs::exists(resolvedPath))
                return false;

            const string cacheKey = Make_NormalizedPathKey(resolvedPath);
            fs::file_time_type writeTime{};
            try
            {
                writeTime = fs::last_write_time(resolvedPath);
            }
            catch (...)
            {
            }

            const auto cachedIter = cache.find(cacheKey);
            if (cachedIter != cache.end() && cachedIter->second.loaded && cachedIter->second.writeTime == writeTime)
            {
                outAsset = cachedIter->second.asset;
                return true;
            }

            Client::UISpriteVfxAsset asset{};
            if (!Client::UISpriteVfxSerializer::Load_FromFile(resolvedPath, asset))
                return false;

            CachedAsset cachedAsset{};
            cachedAsset.asset = asset;
            cachedAsset.writeTime = writeTime;
            cachedAsset.loaded = true;
            cache[cacheKey] = cachedAsset;

            outAsset = asset;
            return true;
        }

        Client::UISpriteVfxAsset Make_ViewportScaledAsset(
            const Client::UISpriteVfxAsset& asset,
            const ImVec2& objectScale)
        {
            Client::UISpriteVfxAsset scaledAsset = asset;
            scaledAsset.spawnRange.x *= objectScale.x;
            scaledAsset.spawnRange.y *= objectScale.y;
            scaledAsset.velocityMin.x *= objectScale.x;
            scaledAsset.velocityMin.y *= objectScale.y;
            scaledAsset.velocityMax.x *= objectScale.x;
            scaledAsset.velocityMax.y *= objectScale.y;
            scaledAsset.sizeStart.x *= objectScale.x;
            scaledAsset.sizeStart.y *= objectScale.y;
            scaledAsset.sizeEnd.x *= objectScale.x;
            scaledAsset.sizeEnd.y *= objectScale.y;
            return scaledAsset;
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
            ImGui::Checkbox("Anchor", &_showAnchors);
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

            const UI_Document& document = _editorManager.Get_Document();
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

            UI_PreviewTransformResolver previewTransformResolver;
            previewTransformResolver.Reset(
                document,
                canvasSize,
                [this](const UI_Object& object, UI_AnimationPreviewSample& outSample)
                {
                    return _editorManager.Sample_AnimationPreview(object, outSample);
                });

            bool hasSelectedObjectRect = false;
            ImVec2 selectedObjectMin = ImVec2(0.f, 0.f);
            ImVec2 selectedObjectMax = ImVec2(0.f, 0.f);
            bool selectedObjectIsLocked = false;

            for (const UI_Object* objectPtr : sortedObjects)
            {
                const UI_Object& object = *objectPtr;
                if (!_editorManager.Is_ObjectEffectivelyVisible(object.id))
                    continue;

                UI_Object renderObject = previewTransformResolver.Make_PreviewObject(object);
                renderObject.color = previewTransformResolver.Get_EffectiveRenderColor(object.id, renderObject.color);
                const ImVec2 resolvedLocalSize = previewTransformResolver.Get_ResolvedLocalSize(object.id);
                const ImVec2 objectWorldPivot = previewTransformResolver.Get_WorldPivot(object.id);
                const UI_WorldObjectFrame objectFrame = previewTransformResolver.Get_WorldFrame(object.id);
                const ImVec2 objectSize = objectFrame.worldSize;
                const float objectWorldRotationRadians = objectFrame.worldRotationRadians;
                const ImVec2 objectMin = Canvas_ToScreen(canvasPos, displayScale, objectFrame.worldBoundsMin);
                const ImVec2 objectMax = Canvas_ToScreen(canvasPos, displayScale, objectFrame.worldBoundsMax);
                const ImVec2 objectAabbMin = Canvas_ToScreen(canvasPos, displayScale, objectFrame.worldAabbMin);
                const ImVec2 objectAabbMax = Canvas_ToScreen(canvasPos, displayScale, objectFrame.worldAabbMax);
                ImVec2 objectQuad[4] = {};
                for (int32 pointIndex = 0; pointIndex < 4; ++pointIndex)
                    objectQuad[pointIndex] = Canvas_ToScreen(canvasPos, displayScale, objectFrame.worldQuad[pointIndex]);
                const bool isSelected = _editorManager.Is_ObjectSelected(object.id);
                const bool isPrimarySelected = (_editorManager.Get_SelectedObjectId() == object.id);
                const bool isEffectivelyLocked = _editorManager.Is_ObjectEffectivelyLocked(object.id);
                const bool shouldDrawAnchorOverlay = !_previewMode && _showAnchors && isSelected;

                if (isPrimarySelected)
                {
                    hasSelectedObjectRect = true;
                    selectedObjectMin = objectAabbMin;
                    selectedObjectMax = objectAabbMax;
                    selectedObjectIsLocked = isEffectivelyLocked;
                }

                const ImU32 fillColor = Is_ContainerObjectType(renderObject.type) ? ImGui::ColorConvertFloat4ToU32(renderObject.color) : Get_ObjectFillColor(renderObject.type);
                const ImU32 objectBorderColor = isPrimarySelected
                    ? (isEffectivelyLocked ? IM_COL32(255, 167, 38, 255) : IM_COL32(255, 217, 102, 255))
                    : (isSelected ? (isEffectivelyLocked ? IM_COL32(255, 183, 77, 245) : IM_COL32(102, 205, 255, 245)) : IM_COL32(210, 214, 220, 220));
                const float borderThickness = isPrimarySelected ? 2.5f : (isSelected ? 2.f : 1.5f);
                const float objectRounding = renderObject.type == UI_ObjectType::Button ? 4.f : 0.f;
                ID3D11ShaderResourceView* previewSRV = nullptr;
                bool requiresCompositePreview = false;
                bool drewUIVfxPreview = false;

                if (renderObject.type == UI_ObjectType::Image && (!renderObject.textureGuid.empty() || !renderObject.texturePath.empty()))
                {
                    if (renderObject.imageRenderMode == UI_ImageRenderMode::MaterialVfx)
                    {
                        requiresCompositePreview = true;
                        MaterialVfxPreviewDesc previewDesc = {};
                        previewDesc.textureGuid = renderObject.textureGuid;
                        previewDesc.texturePath = renderObject.texturePath;
                        previewDesc.maskTextureGuid = renderObject.maskTextureGuid;
                        previewDesc.maskTexturePath = renderObject.maskTexturePath;
                        previewDesc.materialVfx = renderObject.materialVfx;
                        previewSRV = _editorManager.Get_MaterialVfxPreviewSRV(previewDesc);
                    }
                    else if (renderObject.imageRenderMode == UI_ImageRenderMode::ButtonSheen)
                    {
                        requiresCompositePreview = true;
                        ButtonSheenPreviewDesc previewDesc = {};
                        previewDesc.textureGuid = renderObject.textureGuid;
                        previewDesc.texturePath = renderObject.texturePath;
                        previewDesc.maskTextureGuid = renderObject.maskTextureGuid;
                        previewDesc.maskTexturePath = renderObject.maskTexturePath;
                        previewDesc.buttonSheen = renderObject.buttonSheen;
                        previewSRV = _editorManager.Get_ButtonSheenPreviewSRV(previewDesc);
                    }
                    else if (renderObject.imageRenderMode == UI_ImageRenderMode::PatternOverlay)
                    {
                        requiresCompositePreview = true;
                        PatternOverlayPreviewDesc previewDesc = {};
                        previewDesc.textureGuid = renderObject.textureGuid;
                        previewDesc.texturePath = renderObject.texturePath;
                        previewDesc.paintMaskTextureGuid = renderObject.maskTextureGuid;
                        previewDesc.paintMaskTexturePath = renderObject.maskTexturePath;
                        previewDesc.patternMaskTextureGuid = renderObject.patternMaskTextureGuid;
                        previewDesc.patternMaskTexturePath = renderObject.patternMaskTexturePath;
                        previewDesc.patternUvOffset = renderObject.patternUvOffset;
                        previewDesc.patternUvScale = renderObject.patternUvScale;
                        previewDesc.useFillRatio = renderObject.useFillRatio;
                        previewDesc.fillRatio = renderObject.fillRatio;
                        previewDesc.fillOrigin = renderObject.fillOrigin;
                        previewSRV = _editorManager.Get_PatternOverlayPreviewSRV(previewDesc);
                    }
                    else if (renderObject.useFillRatio)
                    {
                        requiresCompositePreview = true;
                        previewSRV = _editorManager.Get_FillRatioPreviewSRV(
                            renderObject.textureGuid,
                            renderObject.texturePath,
                            renderObject.fillRatio,
                            renderObject.fillOrigin);
                    }
                    else if (!renderObject.maskTextureGuid.empty() || !renderObject.maskTexturePath.empty())
                    {
                        requiresCompositePreview = true;
                        previewSRV = _editorManager.Get_MaskedTexturePreviewSRV(
                            renderObject.textureGuid,
                            renderObject.texturePath,
                            renderObject.maskTextureGuid,
                            renderObject.maskTexturePath,
                            renderObject.maskTextureMode);
                    }

                    if (nullptr == previewSRV && !requiresCompositePreview)
                        previewSRV = _editorManager.Get_TexturePreviewSRV(renderObject.textureGuid, renderObject.texturePath);
                }

                if (renderObject.type == UI_ObjectType::UIVfx && !renderObject.vfxPath.empty())
                {
                    Client::UISpriteVfxAsset vfxAsset{};
                    if (Try_LoadUIVfxAsset(renderObject.vfxPath, vfxAsset))
                    {
                        const ImVec2 objectScale(
                            objectSize.x / max(1.f, resolvedLocalSize.x),
                            objectSize.y / max(1.f, resolvedLocalSize.y));
                        const Client::UISpriteVfxAsset scaledAsset = Make_ViewportScaledAsset(vfxAsset, objectScale);
                        ID3D11ShaderResourceView* textureSRV = _editorManager.Get_TexturePreviewSRV(scaledAsset.textureGuid, scaledAsset.texturePath);
                        const ImTextureID textureId = nullptr != textureSRV ? To_ImTextureID(textureSRV) : ImTextureID{};
                        const Vec2 origin(objectWorldPivot.x, objectWorldPivot.y);
                        const float previewTime = static_cast<float>(ImGui::GetTime());
                        const vector<Client::UISpriteVfxParticlePreview> particles =
                            Client::UISpriteVfxPreviewSampler::Sample(scaledAsset, previewTime, origin, true);
                        const float cosAngle = cosf(objectWorldRotationRadians);
                        const float sinAngle = sinf(objectWorldRotationRadians);

                        drawList->PushClipRect(canvasPos, canvasMax, true);
                        for (const Client::UISpriteVfxParticlePreview& particle : particles)
                        {
                            const Vec2 offset = particle.position - origin;
                            const Vec2 rotatedPosition(
                                origin.x + offset.x * cosAngle - offset.y * sinAngle,
                                origin.y + offset.x * sinAngle + offset.y * cosAngle);
                            const ImVec2 particleCenter = Canvas_ToScreen(canvasPos, displayScale, ImVec2(rotatedPosition.x, rotatedPosition.y));
                            const ImVec2 particleSize(
                                max(0.01f, particle.size.x * displayScale),
                                max(0.01f, particle.size.y * displayScale));
                            const ImVec4 color(
                                min(max(particle.color.x, 0.f), 1.f),
                                min(max(particle.color.y, 0.f), 1.f),
                                min(max(particle.color.z, 0.f), 1.f),
                                min(max(particle.color.w, 0.f), 1.f));

                            Draw_RotatedImageOrQuad(
                                drawList,
                                textureId,
                                particleCenter,
                                particleSize,
                                particle.rotationRadians + objectWorldRotationRadians,
                                ImGui::ColorConvertFloat4ToU32(color));
                        }
                        drawList->PopClipRect();
                        drewUIVfxPreview = true;
                    }
                }

                const bool hasTextContent = renderObject.type == UI_ObjectType::Text && !renderObject.text.empty();
                const bool isRotated = fabsf(objectWorldRotationRadians) > kRotationEpsilon;

                if (nullptr != previewSRV)
                {
                    if (isRotated)
                    {
                        drawList->AddImageQuad(
                            To_ImTextureID(previewSRV),
                            objectQuad[0],
                            objectQuad[1],
                            objectQuad[2],
                            objectQuad[3],
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
                else if (!drewUIVfxPreview && !hasTextContent && (!_previewMode || renderObject.type != UI_ObjectType::Image))
                {
                    if (isRotated)
                        drawList->AddQuadFilled(objectQuad[0], objectQuad[1], objectQuad[2], objectQuad[3], fillColor);
                    else
                        drawList->AddRectFilled(objectMin, objectMax, fillColor, objectRounding);
                }

                if (!_previewMode)
                {
                    if (isRotated)
                        Draw_QuadOutline(drawList, objectQuad, objectBorderColor, borderThickness);
                    else
                        drawList->AddRect(objectMin, objectMax, objectBorderColor, objectRounding, 0, borderThickness);

                    if (shouldDrawAnchorOverlay)
                    {
                        const ImVec2 parentReferenceSize = previewTransformResolver.Get_ParentReferenceSize(renderObject);
                        const ImVec2 anchorMinLocal = ImVec2(
                            parentReferenceSize.x * renderObject.anchorMin.x,
                            parentReferenceSize.y * renderObject.anchorMin.y);
                        const ImVec2 anchorMaxLocal = ImVec2(
                            parentReferenceSize.x * renderObject.anchorMax.x,
                            parentReferenceSize.y * renderObject.anchorMax.y);
                        const ImVec2 anchorMinWorld = previewTransformResolver.Transform_LocalPointToWorld(object.id, anchorMinLocal);
                        const ImVec2 anchorMaxWorld = previewTransformResolver.Transform_LocalPointToWorld(object.id, anchorMaxLocal);
                        const ImVec2 anchorMinScreen = ImVec2(
                            canvasPos.x + anchorMinWorld.x * displayScale,
                            canvasPos.y + anchorMinWorld.y * displayScale);
                        const ImVec2 anchorMaxScreen = ImVec2(
                            canvasPos.x + anchorMaxWorld.x * displayScale,
                            canvasPos.y + anchorMaxWorld.y * displayScale);
                        const bool isStretched =
                            fabsf(renderObject.anchorMax.x - renderObject.anchorMin.x) > 0.0001f ||
                            fabsf(renderObject.anchorMax.y - renderObject.anchorMin.y) > 0.0001f;
                        const ImU32 anchorFillColor = isPrimarySelected ? IM_COL32(255, 40, 40, 255) : IM_COL32(255, 40, 40, 210);
                        const ImU32 anchorOutlineColor = IM_COL32(255, 235, 235, 245);
                        if (isStretched)
                        {
                            const ImVec2 anchorRectMin = ImVec2(min(anchorMinScreen.x, anchorMaxScreen.x), min(anchorMinScreen.y, anchorMaxScreen.y));
                            const ImVec2 anchorRectMax = ImVec2(max(anchorMinScreen.x, anchorMaxScreen.x), max(anchorMinScreen.y, anchorMaxScreen.y));
                            drawList->AddRectFilled(anchorRectMin, anchorRectMax, IM_COL32(255, 40, 40, 22), 0.f);
                            drawList->AddRect(anchorRectMin, anchorRectMax, IM_COL32(255, 70, 70, 190), 0.f, 0, 1.25f);
                            Draw_AnchorMarker(drawList, anchorMaxScreen, IM_COL32(255, 120, 120, 190), anchorOutlineColor, isPrimarySelected ? 4.5f : 3.5f);
                        }
                        Draw_AnchorMarker(drawList, anchorMinScreen, anchorFillColor, anchorOutlineColor, isPrimarySelected ? 5.5f : 4.5f);
                    }
                }

                bool usesSpriteFontPreview = false;
                if (!hasBlockingPopup && renderObject.type == UI_ObjectType::Text && (!renderObject.fontGuid.empty() || !renderObject.fontPath.empty()) && !renderObject.text.empty())
                {
                    D3D11_RECT clipRect = {};
                    if (buildViewportClipRect(objectAabbMin, objectAabbMax, clipRect))
                    {
                        const Vec2 fontPivotScreen(
                            canvasPos.x + objectWorldPivot.x * displayScale,
                            canvasPos.y + objectWorldPivot.y * displayScale);
                        const Vec2 fontBoxSize(
                            objectSize.x * displayScale,
                            objectSize.y * displayScale);
                        const Vec2 fontBoxMin(
                            fontPivotScreen.x - fontBoxSize.x * renderObject.pivot.x,
                            fontPivotScreen.y - fontBoxSize.y * renderObject.pivot.y);

                        usesSpriteFontPreview = _editorManager.Queue_FontPreview(
                            renderObject.fontGuid,
                            renderObject.fontPath,
                            String::ToWString(renderObject.text),
                            fontBoxMin,
                            Vec2(fontBoxMin.x + fontBoxSize.x, fontBoxMin.y + fontBoxSize.y),
                            Vec4(renderObject.color.x, renderObject.color.y, renderObject.color.z, renderObject.color.w),
                            objectWorldRotationRadians,
                            fontPivotScreen,
                            renderObject.textAlign,
                            max(0.01f, objectSize.x / max(1.f, resolvedLocalSize.x)) * displayScale,
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
                    _resizeStartObjectWorldPosition = document.Get_WorldPosition(_resizingObjectId);
                    _resizeStartObjectWorldSize = document.Get_WorldSize(_resizingObjectId);
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

                        const UI_WorldObjectFrame objectFrame = Build_UIWorldObjectFrame(document, object.id);
                        const bool isInsideObject = Is_PointInsideUIWorldObjectFrame(mouseCanvasPos, objectFrame);

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
                const UI_Object* resizingObject = document.Find_Object(_resizingObjectId);
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
                        const ImVec2 snappedDelta = Snap_Position(delta, _gridSize);
                        if (movesLeft)
                            nextMin.x = _resizeStartObjectWorldPosition.x + snappedDelta.x;
                        if (movesRight)
                            nextMax.x = _resizeStartObjectWorldPosition.x + _resizeStartObjectWorldSize.x + snappedDelta.x;
                        if (movesTop)
                            nextMin.y = _resizeStartObjectWorldPosition.y + snappedDelta.y;
                        if (movesBottom)
                            nextMax.y = _resizeStartObjectWorldPosition.y + _resizeStartObjectWorldSize.y + snappedDelta.y;
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
                    const UI_Object* draggingObject = document.Find_Object(draggingObjectId);
                    if (nullptr == draggingObject)
                        continue;

                    const auto startPositionIter = _dragStartObjectWorldPositions.find(draggingObjectId);
                    if (startPositionIter == _dragStartObjectWorldPositions.end())
                        continue;

                    const ImVec2 startWorldPosition = startPositionIter->second;
                    ImVec2 moveDelta = delta;
                    if (_snapEnabled)
                        moveDelta = Snap_Position(moveDelta, _gridSize);

                    const ImVec2 nextWorldPosition = ImVec2(
                        startWorldPosition.x + moveDelta.x,
                        startWorldPosition.y + moveDelta.y);

                    _editorManager.Set_UIObjectWorldPosition(draggingObjectId, nextWorldPosition);
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

                            const UI_WorldObjectFrame objectFrame = Build_UIWorldObjectFrame(document, object.id);
                            const bool intersects =
                                objectFrame.worldAabbMin.x <= boxCanvasMax.x &&
                                objectFrame.worldAabbMax.x >= boxCanvasMin.x &&
                                objectFrame.worldAabbMin.y <= boxCanvasMax.y &&
                                objectFrame.worldAabbMax.y >= boxCanvasMin.y;

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
