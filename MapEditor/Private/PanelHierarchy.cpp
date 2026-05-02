#include "pch.h"
#include "PanelHierarchy.h"

PanelHierarchy::PanelHierarchy()
    : MEWindow{ L"Hierarchy" }
{

}

void PanelHierarchy::Render()
{
    bool open = IsOpen();
    if (ImGui::Begin("Hierarchy", &open))
    {
        if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BulletText("(오브젝트 없음)");
            ImGui::TreePop();
        }
    }
    ImGui::End();
    SetOpen(open);
}

Shared<PanelHierarchy> PanelHierarchy::Create()
{
    return make_shared<PanelHierarchy>();
}
