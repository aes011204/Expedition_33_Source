#include "pch.h"
#include "PanelInspector.h"

PanelInspector::PanelInspector()
    : MEWindow{ L"Inspector" }
{

}

void MapEditor::PanelInspector::Render()
{
    bool open = IsOpen();
    if (ImGui::Begin("Inspector", &open))
    {
        ImGui::Text("선택된 오브젝트 없음");
    }
    ImGui::End();
    SetOpen(open);
}

Shared<PanelInspector> PanelInspector::Create()
{
    return make_shared<PanelInspector>();
}
