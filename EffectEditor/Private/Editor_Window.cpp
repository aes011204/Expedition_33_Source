#include "Editor_Window.h"

NS_BEGIN(EffectEditor)

Editor_Window::Editor_Window(const wstring& windowName, const char* windowIcon)
    : _window{ windowName }
{
    const string windowNameText = String::ToString(_window);

    if (nullptr != windowIcon && '\0' != windowIcon[0])
        _imguiWindowName = string{ windowIcon } + " " + windowNameText + "###" + windowNameText;
    else
        _imguiWindowName = windowNameText;
}

HRESULT Editor_Window::Initialize()
{
    return S_OK;
}

void Editor_Window::Update(float)
{
}

void Editor_Window::Pre_Render()
{
}

void Editor_Window::Render()
{
}

bool Editor_Window::CanSave() const
{
    return false;
}

void Editor_Window::Save()
{
}

NS_END
