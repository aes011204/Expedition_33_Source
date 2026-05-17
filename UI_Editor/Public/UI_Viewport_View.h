#pragma once

#include "UI_Editor_Window.h"
#include "UI_Document.h"

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
        void Cancel_InteractionState(bool commitTransaction);

    private:
        UI_Editor_Manager& _editorManager;
        ObjectId _draggingObjectId = 0;
        vector<ObjectId> _draggingObjectIds = {};
        unordered_map<ObjectId, ImVec2> _dragStartObjectWorldPositions = {};
        ObjectId _resizingObjectId = 0;
        UI_ResizeHandle _activeResizeHandle = UI_ResizeHandle::None;
        ImVec2 _dragStartMouseCanvasPos = ImVec2(0.f, 0.f);
        ImVec2 _dragStartObjectWorldPosition = ImVec2(0.f, 0.f);
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
    };
}
