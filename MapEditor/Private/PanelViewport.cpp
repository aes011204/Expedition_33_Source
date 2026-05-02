#include "pch.h"
#include "PanelViewport.h"
#include "RenderTarget.h"

#include "MECamera.h"

PanelViewport::PanelViewport()
    : MEWindow{ L"Viewport" }
{

}

void PanelViewport::Update(float timeDelta)
{
    if (_camera && _isHovered)
    {
        _camera->UpdateInput(timeDelta);
    }
}

void PanelViewport::Render()
{
    bool open = IsOpen();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    if (ImGui::Begin("Viewport", &open))
    {
        const ImVec2 size = ImGui::GetContentRegionAvail();
        _isHovered = ImGui::IsWindowHovered();

        if (_rt)
        {
            const uint32 w = static_cast<uint32>(size.x);
            const uint32 h = static_cast<uint32>(size.y);

            if (w > 0 && h > 0 && (w != _rt->Get_Width() || h != _rt->Get_Height()))
            {
                _rt->Resize(w, h, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4(0.1f, 0.1f, 0.1f, 1.f));
                if (_camera)
                {
                    _camera->SetAspect(static_cast<float>(w) / static_cast<float>(h));
                }
            }

            ImGui::Image(reinterpret_cast<ImTextureID>(_rt->Get_SRV()), size);

            if (_camera && _selectedMatrix)
            {
                const ImVec2 winPos = ImGui::GetWindowPos();
                const ImVec2 winSize = ImGui::GetWindowSize();

                ImGuizmo::SetDrawlist();
                ImGuizmo::SetRect(winPos.x, winPos.y, winSize.x, winSize.y);

                if (ImGui::IsWindowHovered())
                {

                }
            }
        }

        else
        {
            ImGui::Text("No RenderTarget");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    SetOpen(open);
}

Shared<PanelViewport> PanelViewport::Create()
{
    return make_shared<PanelViewport>();
}
