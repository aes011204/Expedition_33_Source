#include "Inspector.h"

#include "Editor_ImGuiInclude.h"

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
        ImGui::TextUnformatted("선택된거 없음");
        ImGui::Separator();
        ImGui::TextDisabled("컴포넌트 추가");
    }
    Set_Open(bOpen);
    ImGui::End();
}

Shared<Inspector> Inspector::Create()
{
    return make_shared<Inspector>();
}



