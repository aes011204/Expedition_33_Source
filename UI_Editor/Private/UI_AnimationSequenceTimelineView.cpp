#include "pch.h"
#include "UI_AnimationSequenceTimelineView.h"

namespace UIEditor
{
    namespace
    {
        constexpr float kRulerHeight = 30.f;
        constexpr float kClipRowMinY = 46.f;
        constexpr float kClipRowHeight = 40.f;
        constexpr float kFrameWidth = 12.f;

        int32 Get_TotalFrames(const UI_AnimationSequenceAsset& sequence)
        {
            if (sequence.clips.empty())
                return 0;

            const UI_AnimationSequenceClip& lastClip = sequence.clips.back();
            return lastClip.startFrame + max(1, lastClip.durationFrames);
        }
    }

    UI_AnimationSequenceTimelineClickResult UI_AnimationSequenceTimelineView::Render(
        const UI_AnimationSequenceAsset& sequence,
        int32 currentFrame,
        int32 selectedClipIndex,
        const ImVec2& size)
    {
        UI_AnimationSequenceTimelineClickResult result = {};
        result.frame = currentFrame;

        const int32 totalFrames = max(1, Get_TotalFrames(sequence));
        const float contentWidth = max(size.x, static_cast<float>(totalFrames) * kFrameWidth + 64.f);
        const float contentHeight = max(size.y, 112.f);

        if (!ImGui::BeginChild("AnimationSequenceTimelineCanvas", size, true, ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGui::EndChild();
            return result;
        }

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("AnimationSequenceTimelineHitArea", ImVec2(contentWidth, contentHeight));
        const bool isCanvasHovered = ImGui::IsItemHovered();
        const bool isCanvasClicked = isCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        const ImVec2 mousePos = ImGui::GetIO().MousePos;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 canvasMax = ImVec2(origin.x + contentWidth, origin.y + contentHeight);
        drawList->AddRectFilled(origin, canvasMax, IM_COL32(31, 31, 34, 255));

        auto frameToX = [&](int32 frame) -> float
        {
            return origin.x + static_cast<float>(frame) * kFrameWidth;
        };

        auto xToFrame = [&](float x) -> int32
        {
            const int32 frame = static_cast<int32>(roundf((x - origin.x) / kFrameWidth));
            return min(max(frame, 0), totalFrames);
        };

        const int32 majorStep = totalFrames > 240 ? 30 : (totalFrames > 120 ? 20 : 10);
        for (int32 frame = 0; frame <= totalFrames; ++frame)
        {
            const float x = frameToX(frame);
            const bool isMajor = (frame % majorStep) == 0 || frame == totalFrames;
            const ImU32 lineColor = isMajor ? IM_COL32(106, 110, 120, 180) : IM_COL32(70, 73, 80, 120);
            drawList->AddLine(ImVec2(x, origin.y), ImVec2(x, canvasMax.y), lineColor, isMajor ? 1.2f : 1.f);

            if (isMajor)
            {
                const string label = to_string(frame);
                drawList->AddText(ImVec2(x + 3.f, origin.y + 5.f), IM_COL32(210, 210, 220, 230), label.c_str());
            }
        }

        drawList->AddLine(
            ImVec2(origin.x, origin.y + kRulerHeight),
            ImVec2(canvasMax.x, origin.y + kRulerHeight),
            IM_COL32(115, 118, 128, 180),
            1.5f);

        const float clipMinY = origin.y + kClipRowMinY;
        const float clipMaxY = clipMinY + kClipRowHeight;
        for (int32 clipIndex = 0; clipIndex < static_cast<int32>(sequence.clips.size()); ++clipIndex)
        {
            const UI_AnimationSequenceClip& clip = sequence.clips[clipIndex];
            const float clipMinX = frameToX(clip.startFrame);
            const float clipMaxX = frameToX(clip.startFrame + max(1, clip.durationFrames));
            const bool isSelected = selectedClipIndex == clipIndex;
            const ImU32 clipColor = isSelected ? IM_COL32(64, 106, 138, 255) : IM_COL32(44, 78, 103, 255);
            drawList->AddRectFilled(ImVec2(clipMinX, clipMinY), ImVec2(clipMaxX, clipMaxY), clipColor, 4.f);
            drawList->AddRect(ImVec2(clipMinX, clipMinY), ImVec2(clipMaxX, clipMaxY), IM_COL32(12, 14, 18, 230), 4.f);

            const string label =
                clip.displayName +
                "  " +
                to_string(clip.asset.fps) +
                "fps/" +
                to_string(clip.asset.endFrame - clip.asset.startFrame) +
                "f";
            const ImVec2 textPos = ImVec2(clipMinX + 8.f, clipMinY + 11.f);
            drawList->AddText(textPos, IM_COL32(236, 240, 245, 245), label.c_str());
        }

        const float currentFrameX = frameToX(min(max(currentFrame, 0), totalFrames));
        drawList->AddLine(
            ImVec2(currentFrameX, origin.y),
            ImVec2(currentFrameX, canvasMax.y),
            IM_COL32(232, 56, 70, 255),
            2.5f);

        if (isCanvasClicked)
        {
            result.clicked = true;
            result.frame = xToFrame(mousePos.x);
            result.clipIndex = Find_ClipIndexAtFrame(sequence, result.frame);
        }

        ImGui::EndChild();
        return result;
    }

    int32 UI_AnimationSequenceTimelineView::Find_ClipIndexAtFrame(const UI_AnimationSequenceAsset& sequence, int32 frame) const
    {
        if (sequence.clips.empty())
            return -1;

        const int32 totalFrames = Get_TotalFrames(sequence);
        const int32 clampedFrame = min(max(frame, 0), totalFrames);
        for (int32 clipIndex = 0; clipIndex < static_cast<int32>(sequence.clips.size()); ++clipIndex)
        {
            const UI_AnimationSequenceClip& clip = sequence.clips[clipIndex];
            if (clampedFrame >= clip.startFrame && clampedFrame < clip.startFrame + max(1, clip.durationFrames))
                return clipIndex;
        }

        return static_cast<int32>(sequence.clips.size()) - 1;
    }
}
