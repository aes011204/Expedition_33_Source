#include "Inspector.h"


Inspector::Inspector()
    : Editor_Window{ L"Inspector" }
{
}

void Inspector::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    if (ImGui::Begin("Inspector", &isOpen))
    {
        ImGui::TextUnformatted("No Selected");
        ImGui::Separator();
        ImGui::TextDisabled("Add Component");
    }
    Set_Open(isOpen);
    ImGui::End();
}

Shared<Inspector> Inspector::Create()
{
    return make_shared<Inspector>();
}



