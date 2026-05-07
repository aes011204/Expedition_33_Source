#include "Emitter_View.h"

NS_BEGIN(EffectEditor)

Emitter_View::Emitter_View()
    : Editor_Window{ L"Emitter" }
{
}

void Emitter_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        ImGui::TextDisabled("Emitter stack will be added here.");
    }

    Set_Open(isOpen);
    ImGui::End();
}

Shared<Emitter_View> Emitter_View::Create()
{
    return make_shared<Emitter_View>();
}

NS_END
