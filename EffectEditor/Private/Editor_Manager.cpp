#include "Editor_Manager.h"
#include "Content_Browser.h"
#include "Editor_Window.h"
#include "EffectEditorInstance.h"
#include "Game_View.h"
#include "Hierarchy_View.h"
#include "Inspector.h"
#include "Scene_View.h"

Editor_Manager::~Editor_Manager()
{
    Free();
}

HRESULT Editor_Manager::Initialize()
{
    Add_Window(Scene_View::Create());
    Add_Window(Game_View::Create());
    Add_Window(Hierarchy_View::Create());
    Add_Window(Content_Browser::Create());
    Add_Window(Inspector::Create());
    return S_OK;
}

void Editor_Manager::Update(float timeDelta)
{
    for (auto& pair : _windows)
    {
        if (nullptr != pair.second)
            pair.second->Update(timeDelta);
    }
}

void Editor_Manager::Render()
{
    Render_DockSpace();

    for (auto& pair : _windows)
    {
        if (nullptr != pair.second)
            pair.second->Render();
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
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (nullptr == viewport)
        return;

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    bool isOpen = true;
    ImGui::Begin("EffectEditorDockSpace", &isOpen, windowFlags);
    ImGui::PopStyleVar(2);

    const ImGuiID dockSpaceID = ImGui::GetID("EffectEditorDockSpaceId");
    ImGui::DockSpace(dockSpaceID, ImVec2(0.f, 0.f), ImGuiDockNodeFlags_PassthruCentralNode);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Window"))
        {
            for (auto& pair : _windows)
            {
                if (nullptr == pair.second)
                    continue;

                bool isWindowOpen = pair.second->Is_Open();
                const string strName = String::ToString(pair.first);
                if (ImGui::MenuItem(strName.c_str(), nullptr, &isWindowOpen))
                    pair.second->Set_Open(isWindowOpen);
            }

            ImGui::EndMenu();
        }

        auto editor = EffectEditorInstance::GetInstance();
        if (nullptr != editor)
        {
            const bool isPlaying = editor->IsPlaying();
            const bool isPaused = editor->IsPaused();

            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::TextDisabled("Run");
            ImGui::SameLine();

            if (isPlaying)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.42f, 0.72f, 1.f));

            if (ImGui::SmallButton("Play"))
            {
                if (isPaused)
                    editor->Resume();
                else
                    editor->Play();
            }

            if (isPlaying)
                ImGui::PopStyleColor();

            ImGui::SameLine();

            if (isPaused)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.46f, 0.10f, 1.f));

            if (ImGui::SmallButton("Pause"))
                editor->Pause();

            if (isPaused)
                ImGui::PopStyleColor();

            ImGui::SameLine();

            if (!isPlaying && !isPaused)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.32f, 0.36f, 0.40f, 1.f));

            if (ImGui::SmallButton("Stop"))
                editor->Stop();

            if (!isPlaying && !isPaused)
                ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();
            ImGui::TextDisabled("Scale");
            ImGui::SameLine();

            float timeScale = editor->Get_RuntimeTimeScale();
            ImGui::SetNextItemWidth(74.f);
            if (ImGui::DragFloat("##RuntimeTimeScale", &timeScale, 0.01f, 0.01f, 8.f, "%.2fx"))
                editor->Set_RuntimeTimeScale(timeScale);

            ImGui::SameLine();
            if (ImGui::SmallButton("1x"))
                editor->Reset_RuntimeTimeScale();
        }

        ImGui::EndMenuBar();
    }

    ImGui::End();
}

Unique<Editor_Manager> Editor_Manager::Create()
{
    Unique<Editor_Manager> instance{ new Editor_Manager() };

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

void Editor_Manager::Free()
{
}
