#include "Editor_Window.h"

Editor_Window::Editor_Window(const wstring& windowName)
    : _window{ windowName }
{
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
