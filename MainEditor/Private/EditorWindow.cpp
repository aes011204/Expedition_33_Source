#include "EditorWindow.h"

#include "Editor_ImGuiInclude.h"
#include "Utils.h"

EditorWindow::EditorWindow(const wstring& strWindowName)
	: _window{ strWindowName }
{
}

void EditorWindow::Update(float fTimeDelta)
{
	UNREFERENCED_PARAMETER(fTimeDelta);
}

void EditorWindow::Render()
{
	if (!_isOpen)
		return;

	const string strTitle = Utils::ToString(_window);
	if (ImGui::Begin(strTitle.c_str(), &_isOpen))
	{
		ImGui::TextUnformatted("Editor window placeholder");
	}
	ImGui::End();
}

void EditorWindow::Set_Open(bool bOpen)
{
	_isOpen = bOpen;
}

const wstring& EditorWindow::Get_WindowName() const
{
	return _window;
}

bool EditorWindow::Is_Open() const
{
	return _isOpen;
}
