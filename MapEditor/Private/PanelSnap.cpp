#include "PanelSnap.h"

PanelSnap::PanelSnap(Shared<SnapSettings> settings)
    : MEWindow{ L"Snap" }, _settings{ settings }
{
    SetOpen(false);
}

void PanelSnap::Render()
{
    bool open = IsOpen();
    if (ImGui::Begin("Snap", &open))
    {
        ImGui::Text("[Ctrl] to activate snapping");
        ImGui::Separator();

        ImGui::DragFloat("Translate##snap", &_settings->translate, 0.05f, 0.01f, 100.f, "%.2f");
        ImGui::DragFloat("Rotate##snap", &_settings->rotate, 0.5f, 1.f, 90.f, "%.1f deg");
        ImGui::DragFloat("Scale##snap", &_settings->scale, 0.01f, 0.01f, 10.f, "%.2f");
    }
    ImGui::End();
    SetOpen(open);
}

Shared<PanelSnap> PanelSnap::Create(Shared<SnapSettings> settings)
{
    return make_shared<PanelSnap>(settings);
}
