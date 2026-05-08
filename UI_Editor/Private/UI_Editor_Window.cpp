#include "pch.h"
#include "UI_Editor_Window.h"

namespace UIEditor
{
    UI_Editor_Window::UI_Editor_Window(const wstring& windowName)
        : _windowName{ windowName }
    {
    }

    HRESULT UI_Editor_Window::Initialize()
    {
        return S_OK;
    }

    void UI_Editor_Window::Render()
    {
    }
}
