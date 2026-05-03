#include "Editor_Manager.h"
#include "EditorInstance.h"
#include "Scene_View.h"
#include "Game_View.h"
#include "Hierarchy_View.h"
#include "Content_Browser.h"
#include "Inspector.h"
#include "Camera_Tool.h"

Editor_Manager::Editor_Manager()
{

}

HRESULT Editor_Manager::Initialize()
{
	Add_Window(Scene_View::Create());
	Add_Window(Game_View::Create());
	Add_Window(Hierarchy_View::Create());
	Add_Window(Content_Browser::Create());
	Add_Window(Inspector::Create());
    Add_Window(Camera_Tool::Create());
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

void Editor_Manager::Add_Window(const Shared<Editor_Window>& window)
{
	if (nullptr == window)
		return;

	_windows[window->Get_WindowName()] = window;
}

Shared<Editor_Window> Editor_Manager::Find_Window(const wstring& windowName) const
{
	const auto iter = _windows.find(windowName);
	if (iter == _windows.end())
		return nullptr;

	return iter->second;
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
				const string strName = String::ToString(Pair.first);
				if (ImGui::MenuItem(strName.c_str(), nullptr, &bWindowOpen))
					Pair.second->Set_Open(bWindowOpen);
			}

			ImGui::EndMenu();
		}

		if (nullptr != EDITOR)
		{
			const bool playing = EDITOR->IsPlaying();
			const bool paused = EDITOR->IsPaused();

			ImGui::SameLine();
			ImGui::Spacing();
			ImGui::SameLine();
			ImGui::TextDisabled("|");
			ImGui::SameLine();
			ImGui::TextDisabled("Run");
			ImGui::SameLine();

			if (playing)
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.42f, 0.72f, 1.f));

			if (ImGui::SmallButton("Play"))
			{
				if (paused)
					EDITOR->Resume();
				else
					EDITOR->Play();
			}

			if (playing)
				ImGui::PopStyleColor();

			ImGui::SameLine();

			if (paused)
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.46f, 0.10f, 1.f));

			if (ImGui::SmallButton("Pause"))
				EDITOR->Pause();

			if (paused)
				ImGui::PopStyleColor();

			ImGui::SameLine();

			if (!playing && !paused)
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.32f, 0.36f, 0.40f, 1.f));

			if (ImGui::SmallButton("Stop"))
				EDITOR->Stop();

			if (!playing && !paused)
				ImGui::PopStyleColor();

			ImGui::SameLine();
			ImGui::Spacing();
			ImGui::SameLine();
			ImGui::TextDisabled("Scale");
			ImGui::SameLine();

			float fTimeScale = EDITOR->Get_RuntimeTimeScale();
			ImGui::SetNextItemWidth(74.f);
			if (ImGui::DragFloat("##RuntimeTimeScale", &fTimeScale, 0.01f, 0.01f, 8.f, "%.2fx"))
				EDITOR->Set_RuntimeTimeScale(fTimeScale);

			ImGui::SameLine();
			if (ImGui::SmallButton("1x"))
				EDITOR->Reset_RuntimeTimeScale();
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
