#include "Game_View.h"
#include "GameInstance.h"
#include "EditorInstance.h"
#include "RenderTarget.h"

Game_View::Game_View()
    : Editor_Window{ L"Game" }
{
}

void Game_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (EDITOR->Consume_WindowFocusRequest(EDITOR_VIEWPORT_TARGET::Game))
        ImGui::SetNextWindowFocus();

    if (ImGui::Begin(windowName.c_str(), &isOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        if (viewportSize.x < 1.f)
            viewportSize.x = 1.f;

        if (viewportSize.y < 1.f)
            viewportSize.y = 1.f;

        const uint32 width = static_cast<uint32>(viewportSize.x);
        const uint32 height = static_cast<uint32>(viewportSize.y);

        EditorInstance::GetInstance()->Request_ViewportSize(
            EDITOR_VIEWPORT_TARGET::Game,
            width,
            height);

        ID3D11ShaderResourceView* pSRV = EditorInstance::GetInstance()->Get_SharedViewportSRV();
        if (nullptr != pSRV)
        {
            const ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(pSRV));
            ImGui::Image(textureId, viewportSize);
        }
        else
        {
            ImGui::TextUnformatted("Game RenderTarget Missing");
        }
    }
    Set_Open(isOpen);
    ImGui::End();
}

Shared<Game_View> Game_View::Create()
{
    return make_shared<Game_View>();
}



