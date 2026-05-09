#include "ImSequencer.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace ImSequencer
{
namespace
{
    constexpr float kLegendWidth = 200.f;
    constexpr float kHeaderHeight = 20.f;
    constexpr float kItemHeight = 30.f;
    constexpr float kMinHeight = 120.f;

    float Frame_ToX(int frame, int frameMin, int frameMax, float trackMinX, float trackWidth)
    {
        const float frameRange = static_cast<float>(std::max(1, frameMax - frameMin));
        const float t = static_cast<float>(frame - frameMin) / frameRange;
        return trackMinX + std::clamp(t, 0.f, 1.f) * trackWidth;
    }

    int X_ToFrame(float x, int frameMin, int frameMax, float trackMinX, float trackWidth)
    {
        const float t = std::clamp((x - trackMinX) / std::max(1.f, trackWidth), 0.f, 1.f);
        return frameMin + static_cast<int>(std::round(t * static_cast<float>(std::max(1, frameMax - frameMin))));
    }

    int Pick_LabelStep(int frameMin, int frameMax, float trackWidth)
    {
        const int frameRange = std::max(1, frameMax - frameMin);
        int step = 1;
        while ((trackWidth / static_cast<float>(frameRange) * static_cast<float>(step)) < 64.f)
            step *= 2;

        if (step > 10)
        {
            const int rounded = ((step + 9) / 10) * 10;
            step = std::max(10, rounded);
        }

        return std::max(1, step);
    }
}

bool Sequencer(SequenceInterface* sequence, int* currentFrame, bool* expanded, int* selectedEntry, int* firstFrame, int sequenceOptions)
{
    if (sequence == nullptr)
        return false;

    if (expanded != nullptr)
        *expanded = true;

    if (firstFrame != nullptr)
        *firstFrame = sequence->GetFrameMin();

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window == nullptr || window->SkipItems)
        return false;

    const int frameMin = sequence->GetFrameMin();
    const int frameMax = std::max(frameMin + 1, sequence->GetFrameMax());
    const int itemCount = sequence->GetItemCount();

    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.x = std::max(canvasSize.x, kLegendWidth + 120.f);
    canvasSize.y = std::max(canvasSize.y, std::max(kMinHeight, kHeaderHeight + kItemHeight * static_cast<float>(std::max(1, itemCount)) + 10.f));

    ImGui::InvisibleButton(
        "##ImSequencerCanvas",
        canvasSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    bool changedFrame = false;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImRect canvasRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y));
    const float trackMinX = canvasRect.Min.x + kLegendWidth;
    const float trackMaxX = canvasRect.Max.x - 4.f;
    const float trackWidth = std::max(1.f, trackMaxX - trackMinX);
    const float framePixelWidth = trackWidth / static_cast<float>(std::max(1, frameMax - frameMin));
    const ImRect clipRect(canvasRect.Min, canvasRect.Max);

    sequence->focused = hovered || active;

    drawList->PushClipRect(clipRect.Min, clipRect.Max, true);
    drawList->AddRectFilled(canvasRect.Min, canvasRect.Max, IM_COL32(30, 30, 30, 255));
    drawList->AddRect(canvasRect.Min, canvasRect.Max, IM_COL32(76, 78, 84, 255));
    drawList->AddRectFilled(canvasRect.Min, ImVec2(trackMinX, canvasRect.Max.y), IM_COL32(32, 32, 34, 255));
    drawList->AddRectFilled(ImVec2(trackMinX, canvasRect.Min.y), ImVec2(canvasRect.Max.x, canvasRect.Min.y + kHeaderHeight), IM_COL32(56, 57, 64, 255));

    const int labelStep = Pick_LabelStep(frameMin, frameMax, trackWidth);
    auto drawFrameLine = [&](int frame, bool major)
    {
        const float x = Frame_ToX(frame, frameMin, frameMax, trackMinX, trackWidth);
        const ImU32 color = major ? IM_COL32(115, 118, 128, 160) : IM_COL32(88, 90, 98, 105);
        const float y0 = canvasRect.Min.y + (major ? 0.f : kHeaderHeight * 0.55f);
        drawList->AddLine(ImVec2(x, y0), ImVec2(x, canvasRect.Max.y), color);
    };

    for (int frame = frameMin; frame <= frameMax; frame += std::max(1, labelStep / 2))
        drawFrameLine(frame, frame == frameMin || frame == frameMax || (frame % labelStep) == 0);

    if ((frameMax % std::max(1, labelStep / 2)) != 0)
        drawFrameLine(frameMax, true);

    auto drawFrameLabel = [&](int frame, bool alignRight)
    {
        const std::string label = std::to_string(frame);
        const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        float x = Frame_ToX(frame, frameMin, frameMax, trackMinX, trackWidth) + 3.f;
        if (alignRight)
            x = Frame_ToX(frame, frameMin, frameMax, trackMinX, trackWidth) - textSize.x - 3.f;

        drawList->AddText(ImVec2(x, canvasRect.Min.y + 2.f), IM_COL32(190, 190, 196, 255), label.c_str());
    };

    drawFrameLabel(frameMin, false);
    for (int frame = frameMin + labelStep; frame < frameMax; frame += labelStep)
        drawFrameLabel(frame, false);
    drawFrameLabel(frameMax, true);

    for (int i = 0; i < itemCount; ++i)
    {
        const float rowY = canvasRect.Min.y + kHeaderHeight + kItemHeight * static_cast<float>(i);
        const ImRect rowRect(ImVec2(canvasRect.Min.x, rowY), ImVec2(canvasRect.Max.x, rowY + kItemHeight));
        const ImRect legendRect(rowRect.Min, ImVec2(trackMinX, rowRect.Max.y));
        const ImRect trackRect(ImVec2(trackMinX, rowY), ImVec2(trackMaxX, rowY + kItemHeight));

        const bool rowSelected = selectedEntry != nullptr && *selectedEntry == i;
        drawList->AddRectFilled(rowRect.Min, rowRect.Max, rowSelected ? IM_COL32(54, 58, 66, 255) : IM_COL32(31, 31, 31, 255));
        drawList->AddLine(ImVec2(canvasRect.Min.x, rowRect.Min.y), ImVec2(canvasRect.Max.x, rowRect.Min.y), IM_COL32(50, 50, 54, 160));

        if (const char* label = sequence->GetItemLabel(i); label != nullptr && label[0] != '\0')
            drawList->AddText(ImVec2(legendRect.Min.x + 8.f, legendRect.Min.y + 4.f), IM_COL32(230, 230, 232, 255), label);

        int* start = nullptr;
        int* end = nullptr;
        int type = 0;
        unsigned int color = IM_COL32(72, 72, 72, 255);
        sequence->Get(i, &start, &end, &type, &color);

        if (start != nullptr && end != nullptr)
        {
            const float slotStartX = Frame_ToX(*start, frameMin, frameMax, trackMinX, trackWidth);
            const float slotEndX = Frame_ToX(*end, frameMin, frameMax, trackMinX, trackWidth);
            drawList->AddRectFilled(
                ImVec2(slotStartX, trackRect.Min.y + 3.f),
                ImVec2(std::max(slotStartX + framePixelWidth, slotEndX), trackRect.Max.y - 3.f),
                color,
                2.f);
        }

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && rowRect.Contains(ImGui::GetIO().MousePos))
        {
            if (selectedEntry != nullptr)
                *selectedEntry = i;
        }

        sequence->CustomDraw(i, drawList, trackRect, legendRect, clipRect, clipRect);
    }

    if (currentFrame != nullptr)
    {
        *currentFrame = std::clamp(*currentFrame, frameMin, frameMax);

        if ((sequenceOptions & SEQUENCER_CHANGE_FRAME) != 0 && hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const ImVec2 mousePos = ImGui::GetIO().MousePos;
            if (mousePos.x >= trackMinX && mousePos.x <= trackMaxX)
            {
                const int newFrame = X_ToFrame(mousePos.x, frameMin, frameMax, trackMinX, trackWidth);
                if (newFrame != *currentFrame)
                {
                    *currentFrame = newFrame;
                    changedFrame = true;
                }
            }
        }

        const float cursorX = Frame_ToX(*currentFrame, frameMin, frameMax, trackMinX, trackWidth);
        drawList->AddRectFilled(
            ImVec2(cursorX - 3.f, canvasRect.Min.y),
            ImVec2(cursorX + 3.f, canvasRect.Min.y + kHeaderHeight + kItemHeight * static_cast<float>(itemCount)),
            IM_COL32(235, 54, 42, 210));
    }

    drawList->PopClipRect();
    return changedFrame;
}
}
