#pragma once

#include "UI_Editor_Window.h"
#include "UI_Document.h"

namespace UIEditor
{
    class UI_Editor_Manager;

    class UI_Content_Browser final : public UI_Editor_Window
    {
    public:
        explicit UI_Content_Browser(UI_Editor_Manager& editorManager);
        ~UI_Content_Browser() override = default;

    public:
        void Render() override;

    private:
        void Render_TextureTab(const string& searchText, UI_Object* selectedObject);
        void Render_FontTab(const string& searchText, UI_Object* selectedObject);

    public:
        static Shared<UI_Content_Browser> Create(UI_Editor_Manager& editorManager);

    private:
        UI_Editor_Manager& _editorManager;
        char _contentBrowserSearchBuffer[128] = {};
    };
}
