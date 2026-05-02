#include "Editor_Window.h"

Editor_Window::Editor_Window(const wstring& windowName)
	: _window{ windowName }
{
}

void Editor_Window::Update(float timeDelta)
{
}

void Editor_Window::Render()
{
	if (!_isOpen)
		return;

	const string strTitle = String::ToString(_window);
	if (ImGui::Begin(strTitle.c_str(), &_isOpen))
	{
		ImGui::TextUnformatted("Editor window placeholder");
	}
	ImGui::End();
}

void Editor_Window::Set_Open(bool isOpen)
{
	_isOpen = isOpen;
}

const wstring& Editor_Window::Get_WindowName() const
{
	return _window;
}

bool Editor_Window::Is_Open() const
{
	return _isOpen;
}
