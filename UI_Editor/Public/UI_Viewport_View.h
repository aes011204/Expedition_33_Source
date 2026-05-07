#pragma once

#include "UI_Editor_Window.h"
#include "UI_Document.h"

namespace UIEditor
{
    class UI_Editor_Manager;

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
        UI_Editor_Manager& _editorManager;
        ObjectId _draggingObjectId = 0;
        ImVec2 _dragStartMouseCanvasPos = ImVec2(0.f, 0.f);
        ImVec2 _dragStartObjectPosition = ImVec2(0.f, 0.f);
        bool _isPanningViewport = false;
        float _zoomFactor = 1.f;
    };
}
