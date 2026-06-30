#pragma once

#include "UI_EditorLayoutCache.h"
#include "UI_Editor_Window.h"
#include "UI_Document.h"
#include "UI_ViewportPreviewAssetService.h"

namespace UIEditor
{
    class UI_Editor_Manager;

    enum class UI_ResizeHandle
    {
        None,
        North,
        NorthEast,
        East,
        SouthEast,
        South,
        SouthWest,
        West,
        NorthWest,
    };

    class UI_Viewport_View final : public UI_Editor_Window
    {
    public:
        explicit UI_Viewport_View(UI_Editor_Manager& editorManager);
        ~UI_Viewport_View() override = default;

    public:
        void Render() override;

    public:
        static Shared<UI_Viewport_View> Create(UI_Editor_Manager& editorManager);

    private:
        void Render_Toolbar();
        void Draw_ViewportItems(
            ImDrawList* drawList,
            const ImVec2& canvasPos,
            const ImVec2& canvasMax,
            float displayScale,
            const ImVec2& viewportMin,
            const ImVec2& viewportMax,
            bool hasBlockingPopup,
            bool useInteractionTextLod,
            const UI_PreviewTransformResolver& previewTransformResolver,
            const vector<UI_EditorViewportItem>& viewportItems,
            bool& outHasSelectedObjectRect,
            ImVec2& outSelectedObjectMin,
            ImVec2& outSelectedObjectMax,
            bool& outSelectedObjectIsLocked);
        void Handle_ViewportInteraction(
            const UI_Document& document,
            const UI_EditorReadModel& readModel,
            const vector<UI_EditorViewportItem>& viewportItems,
            ImDrawList* drawList,
            const ImVec2& canvasSize,
            const ImVec2& canvasPos,
            const ImVec2& canvasMax,
            float displayScale,
            const ImVec2& mousePos,
            const ImVec2& mouseCanvasPos,
            bool hasBlockingPopup,
            bool viewportInputAllowed,
            bool isMouseInViewport,
            bool isMouseInCanvas,
            bool isAnimationPreviewActive,
            UI_ResizeHandle hoveredResizeHandle);
        void Cancel_InteractionState(bool commitTransaction);

    private:
        UI_Editor_Manager& _editorManager;
        ObjectId _draggingObjectId = 0;
        vector<ObjectId> _draggingObjectIds = {};
        unordered_map<ObjectId, ImVec2> _dragStartObjectWorldPositions = {};
        ObjectId _resizingObjectId = 0;
        UI_ResizeHandle _activeResizeHandle = UI_ResizeHandle::None;
        ImVec2 _dragStartMouseCanvasPos = ImVec2(0.f, 0.f);
        ImVec2 _resizeStartMouseCanvasPos = ImVec2(0.f, 0.f);
        ImVec2 _resizeStartObjectWorldPosition = ImVec2(0.f, 0.f);
        ImVec2 _resizeStartObjectWorldSize = ImVec2(0.f, 0.f);
        bool _isPanningViewport = false;
        bool _isBoxSelecting = false;
        ImVec2 _boxSelectStartCanvasPos = ImVec2(0.f, 0.f);
        ImVec2 _boxSelectCurrentCanvasPos = ImVec2(0.f, 0.f);
        float _zoomFactor = 1.f;
        float _gridSize = 16.f;
        bool _previewMode = false;
        bool _showGrid = true;
        bool _showAnchors = false;
        bool _snapEnabled = false;
        UI_ViewportPreviewAssetService _previewAssetService = {};
    };
}
