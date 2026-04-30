#include "Inspector.h"

#include "Editor_ImGuiInclude.h"

Inspector::Inspector()
    : EditorWindow{ L"Inspector" }
{
}

void Inspector::Render()
{
    if (!Is_Open())
        return;

    bool bOpen = Is_Open();
    if (ImGui::Begin("Inspector", &bOpen))
    {
        ImGui::TextUnformatted("Nothing selected");
        ImGui::Separator();
        ImGui::TextDisabled("Transform, component, and asset inspectors will attach here.");
    }
    Set_Open(bOpen);
    ImGui::End();
}

Shared<Inspector> Inspector::Create()
{
    return make_shared<Inspector>();
}



