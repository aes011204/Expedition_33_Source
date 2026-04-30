#include "Editor_Manager.h"

#include "Editor_ImGuiInclude.h"
#include "Utils.h"
#include "Scene_View.h"
#include "Game_View.h"
#include "Hierarchy_View.h"
#include "Content_Browser.h"
#include "Inspector.h"

HRESULT Editor_Manager::Initialize()
{
	Register_DefaultWindows();

	return S_OK;
}

void Editor_Manager::Update(float timeDelta)
{
	for (auto& Pair : _windows)
	{
		if (nullptr != Pair.second)
			Pair.second->Update(timeDelta);
	}
}

void Editor_Manager::Render()
{
	Render_DockSpace();

	for (auto& Pair : _windows)
	{
		if (nullptr != Pair.second)
			Pair.second->Render();
	}
}

void Editor_Manager::Register_Window(const Shared<EditorWindow>& window)
{
	if (nullptr == window)
		return;

	_windows[window->Get_WindowName()] = window;
}

Shared<EditorWindow> Editor_Manager::Find_Window(const wstring& windowName) const
{
	const auto iter = _windows.find(windowName);
	if (iter == _windows.end())
		return nullptr;

	return iter->second;
}

void Editor_Manager::Register_DefaultWindows()
{
	Register_Window(make_shared<Scene_View>());
	Register_Window(make_shared<Game_View>());
	Register_Window(make_shared<Hierarchy_View>());
	Register_Window(make_shared<Content_Browser>());
	Register_Window(make_shared<Inspector>());
}

void Editor_Manager::Render_DockSpace()
{
	ImGuiViewport* pViewport = ImGui::GetMainViewport();
	if (nullptr == pViewport)
		return;

	ImGui::SetNextWindowPos(pViewport->WorkPos);
	ImGui::SetNextWindowSize(pViewport->WorkSize);
	ImGui::SetNextWindowViewport(pViewport->ID);

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	WindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	WindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

	bool bOpen = true;
	ImGui::Begin("MainEditorDockSpace", &bOpen, WindowFlags);
	ImGui::PopStyleVar(2);

	const ImGuiID DockSpaceId = ImGui::GetID("MainEditorDockSpaceId");
	ImGui::DockSpace(DockSpaceId, ImVec2(0.f, 0.f), ImGuiDockNodeFlags_PassthruCentralNode);

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Window"))
		{
			for (auto& Pair : _windows)
			{
				if (nullptr == Pair.second)
					continue;

				bool bWindowOpen = Pair.second->Is_Open();
				const string strName = Utils::ToString(Pair.first);
				if (ImGui::MenuItem(strName.c_str(), nullptr, &bWindowOpen))
					Pair.second->Set_Open(bWindowOpen);
			}

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	ImGui::End();
}

Unique<Editor_Manager> Editor_Manager::Create()
{
	Unique<Editor_Manager> pInstance{ new Editor_Manager() };

	if (FAILED(pInstance->Initialize()))
		return nullptr;

	return pInstance;
}
