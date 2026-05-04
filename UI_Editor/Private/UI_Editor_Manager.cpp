#include "pch.h"
#include "UI_Editor_Manager.h"

HRESULT UI_Editor_Manager::Initialize()
{
    return S_OK;
}

void UI_Editor_Manager::Render()
{
    Render_DockSpace();
    Render_Test_Window();
}

void UI_Editor_Manager::Render_DockSpace()
{
    ImGuiViewport* pViewport = ImGui::GetMainViewport();
    if (nullptr == pViewport)
        return;

    ImGui::SetNextWindowPos(pViewport->WorkPos);
    ImGui::SetNextWindowSize(pViewport->WorkSize);
    ImGui::SetNextWindowViewport(pViewport->ID);

    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    WindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    bool bOpen = true;
    ImGui::Begin("UIEditorDockSpace", &bOpen, WindowFlags);
    ImGui::PopStyleVar(2);

    const ImGuiID DockSpaceId = ImGui::GetID("UIEditorDockSpaceId");
    ImGui::DockSpace(DockSpaceId, ImVec2(0,0), ImGuiDockNodeFlags_None);
}

void UI_Editor_Manager::Render_Test_Window()
{
    ImGui::Begin("UI_Editor_Test");
    ImGui::Text("UI_Editor_Manager 연결");
    ImGui::TextDisabled("Complite Test");
    ImGui::End();
}

Unique<UI_Editor_Manager> UI_Editor_Manager::Create()
{
    Unique<UI_Editor_Manager> pInstance{new UI_Editor_Manager()};

    if (FAILED(pInstance->Initialize()))
        return nullptr;

    return pInstance;
}
