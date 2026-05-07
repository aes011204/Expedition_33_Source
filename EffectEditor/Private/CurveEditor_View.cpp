#include "CurveEditor_View.h"

NS_BEGIN(EffectEditor)

CurveEditor_View::CurveEditor_View()
    : Editor_Window{ L"Curve Editor" }
{
}

void CurveEditor_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        ImGui::TextDisabled("Curve editor will be added here.");
    }

    Set_Open(isOpen);
    ImGui::End();
}

Shared<CurveEditor_View> CurveEditor_View::Create()
{
    return make_shared<CurveEditor_View>();
}

NS_END
