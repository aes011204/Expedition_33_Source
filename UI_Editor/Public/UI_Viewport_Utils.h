#pragma once

#include "UI_Viewport_View.h"

namespace UIEditor::ViewportUtils
{
    constexpr float kMinViewportZoomFactor = 1.f;
    constexpr float kMaxViewportZoomFactor = 16.f;
    constexpr float kViewportZoomStep = 1.1f;
    constexpr float kMinGridSize = 2.f;
    constexpr float kMaxGridSize = 256.f;

    float Clamp_Zoom(float zoomFactor);
    ImVec2 Snap_Position(const ImVec2& position, float gridSize);
    float Snap_Value(float value, float gridSize);
    ImVec2 Get_ResizeHandleCenter(UI_ResizeHandle resizeHandle, const ImVec2& objectMin, const ImVec2& objectMax);
    UI_ResizeHandle Hit_TestResizeHandle(const ImVec2& mousePosition, const ImVec2& objectMin, const ImVec2& objectMax);
    void Draw_ResizeHandles(ImDrawList* drawList, const ImVec2& objectMin, const ImVec2& objectMax);
    void Set_ResizeMouseCursor(UI_ResizeHandle resizeHandle);
    void Draw_ImGuiTextPreview(
        ImDrawList* drawList,
        const UI_Object& object,
        const ImVec2& objectMin,
        const ImVec2& objectMax,
        float displayScaleX);
}
