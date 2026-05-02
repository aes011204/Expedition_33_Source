#include "Inspector.h"

Inspector::Inspector()
    : Editor_Window{ L"Inspector" }
{
}

void Inspector::Render()
{
    if (!Is_Open())
        return;

    bool bOpen = Is_Open();
    if (ImGui::Begin("Inspector", &bOpen))
    {
        ImGui::TextUnformatted("No Selected");
        ImGui::Separator();
        ImGui::TextDisabled("Add Component");
    }
    Set_Open(bOpen);
    ImGui::End();
}

Shared<Inspector> Inspector::Create()
{
    return make_shared<Inspector>();
}
