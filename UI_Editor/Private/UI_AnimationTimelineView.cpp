#include "pch.h"
#include "UI_AnimationTimelineView.h"

namespace UIEditor
{
    namespace
    {
        constexpr float kTrackPanelWidth = 255.f;
        constexpr float kKeyPanelWidth = 220.f;
        constexpr float kRulerHeight = 30.f;
        constexpr float kTrackRowHeight = 34.f;
        constexpr float kFrameWidth = 18.f;
    }

    float UI_AnimationTimelineView::Get_TrackPanelWidth()
    {
        return kTrackPanelWidth;
    }

    float UI_AnimationTimelineView::Get_KeyPanelWidth()
    {
        return kKeyPanelWidth;
    }

    UI_AnimationTimelineClickResult UI_AnimationTimelineView::Render(
        const UI_AnimationAsset& asset,
        int32 currentFrame,
        int32 selectedTrackIndex,
        int32 selectedKeyIndex,
        const ImVec2& size)
    {
        UI_AnimationTimelineClickResult result = {};
        result.frame = currentFrame;

        const int32 frameCount = max(1, asset.endFrame - asset.startFrame + 1);
        const float contentWidth = max(size.x, static_cast<float>(frameCount) * kFrameWidth + 32.f);
        const float contentHeight = max(size.y, kRulerHeight + max(1, static_cast<int32>(asset.tracks.size())) * kTrackRowHeight + 16.f);

        if (!ImGui::BeginChild("AnimationTimelineCanvas", size, true, ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGui::EndChild();
            return result;
        }

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("AnimationTimelineHitArea", ImVec2(contentWidth, contentHeight));
        const bool isCanvasHovered = ImGui::IsItemHovered();
        const bool isCanvasClicked = isCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        const ImVec2 mousePos = ImGui::GetIO().MousePos;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 canvasMax = ImVec2(origin.x + contentWidth, origin.y + contentHeight);
        drawList->AddRectFilled(origin, canvasMax, IM_COL32(31, 31, 34, 255));

        auto frameToX = [&](int32 frame) -> float
        {
            return origin.x + static_cast<float>(frame - asset.startFrame) * kFrameWidth;
        };

        auto xToFrame = [&](float x) -> int32
        {
            const int32 frame = asset.startFrame + static_cast<int32>(roundf((x - origin.x) / kFrameWidth));
            return min(max(frame, asset.startFrame), asset.endFrame);
        };

        auto hitTestKey = [&](const ImVec2& screenPos, int32& outTrackIndex, int32& outKeyIndex) -> bool
        {
            outTrackIndex = -1;
            outKeyIndex = -1;

            if (screenPos.y < origin.y + kRulerHeight)
                return false;

            const int32 trackIndex = static_cast<int32>((screenPos.y - origin.y - kRulerHeight) / kTrackRowHeight);
            if (trackIndex < 0 || trackIndex >= static_cast<int32>(asset.tracks.size()))
                return false;

            const UI_AnimationKeyframeTrack& track = asset.tracks[trackIndex];
            for (int32 keyIndex = 0; keyIndex < static_cast<int32>(track.keys.size()); ++keyIndex)
            {
                const float keyX = frameToX(track.keys[keyIndex].frame);
                if (fabsf(screenPos.x - keyX) <= 7.f)
                {
                    outTrackIndex = trackIndex;
                    outKeyIndex = keyIndex;
                    return true;
                }
            }

            return false;
        };

        const int32 majorStep = frameCount > 240 ? 30 : (frameCount > 120 ? 20 : 10);
        for (int32 frame = asset.startFrame; frame <= asset.endFrame; ++frame)
        {
            const float x = frameToX(frame);
            const bool isMajor = ((frame - asset.startFrame) % majorStep) == 0 || frame == asset.endFrame;
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

        for (int32 trackIndex = 0; trackIndex < static_cast<int32>(asset.tracks.size()); ++trackIndex)
        {
            const UI_AnimationKeyframeTrack& track = asset.tracks[trackIndex];
            const float rowMinY = origin.y + kRulerHeight + static_cast<float>(trackIndex) * kTrackRowHeight;
            const float rowMaxY = rowMinY + kTrackRowHeight;
            const bool isSelectedTrack = selectedTrackIndex == trackIndex;
            const ImU32 rowColor = isSelectedTrack ? IM_COL32(62, 66, 76, 255) : (trackIndex % 2 == 0 ? IM_COL32(42, 43, 48, 255) : IM_COL32(37, 38, 42, 255));
            drawList->AddRectFilled(ImVec2(origin.x, rowMinY), ImVec2(canvasMax.x, rowMaxY), rowColor);
            drawList->AddText(
                ImVec2(origin.x + 8.f, rowMinY + 9.f),
                IM_COL32(222, 224, 232, 235),
                Get_AnimationKeyframeTrackTypeName(track.type));

            for (int32 keyIndex = 0; keyIndex < static_cast<int32>(track.keys.size()); ++keyIndex)
            {
                const UI_AnimationKeyframe& key = track.keys[keyIndex];
                const float x = frameToX(key.frame);
                const float y = rowMinY + kTrackRowHeight * 0.5f;
                const bool isSelectedKey = selectedTrackIndex == trackIndex && selectedKeyIndex == keyIndex;
                const ImU32 keyColor = isSelectedKey ? IM_COL32(255, 230, 0, 255) : IM_COL32(255, 255, 255, 245);
                drawList->AddRectFilled(ImVec2(x - 5.f, y - 8.f), ImVec2(x + 5.f, y + 8.f), keyColor, 2.f);
                drawList->AddRect(ImVec2(x - 5.f, y - 8.f), ImVec2(x + 5.f, y + 8.f), IM_COL32(0, 0, 0, 220), 2.f);
            }
        }

        const float currentFrameX = frameToX(currentFrame);
        drawList->AddLine(
            ImVec2(currentFrameX, origin.y),
            ImVec2(currentFrameX, canvasMax.y),
            IM_COL32(232, 56, 70, 255),
            2.5f);

        if (isCanvasClicked)
        {
            result.clicked = true;
            result.frame = xToFrame(mousePos.x);
            _draggingKey = false;
            _dragTrackIndex = -1;
            _dragKeyIndex = -1;

            if (mousePos.y >= origin.y + kRulerHeight)
            {
                result.trackIndex = static_cast<int32>((mousePos.y - origin.y - kRulerHeight) / kTrackRowHeight);
                if (result.trackIndex < 0 || result.trackIndex >= static_cast<int32>(asset.tracks.size()))
                    result.trackIndex = -1;
            }

            if (result.trackIndex >= 0)
            {
                int32 hitTrackIndex = -1;
                int32 hitKeyIndex = -1;
                if (hitTestKey(mousePos, hitTrackIndex, hitKeyIndex))
                {
                    result.hitKey = true;
                    result.trackIndex = hitTrackIndex;
                    result.keyIndex = hitKeyIndex;
                    result.frame = asset.tracks[hitTrackIndex].keys[hitKeyIndex].frame;

                    _draggingKey = true;
                    _dragTrackIndex = hitTrackIndex;
                    _dragKeyIndex = hitKeyIndex;
                }
            }
        }

        if (_draggingKey)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                const bool useSelectedKey = selectedTrackIndex == _dragTrackIndex && selectedKeyIndex >= 0;
                const int32 targetTrackIndex = useSelectedKey ? selectedTrackIndex : _dragTrackIndex;
                const int32 targetKeyIndex = useSelectedKey ? selectedKeyIndex : _dragKeyIndex;
                const int32 targetFrame = xToFrame(mousePos.x);

                if (targetTrackIndex >= 0 &&
                    targetTrackIndex < static_cast<int32>(asset.tracks.size()) &&
                    targetKeyIndex >= 0 &&
                    targetKeyIndex < static_cast<int32>(asset.tracks[targetTrackIndex].keys.size()))
                {
                    const int32 currentKeyFrame = asset.tracks[targetTrackIndex].keys[targetKeyIndex].frame;
                    const float rowMinY = origin.y + kRulerHeight + static_cast<float>(targetTrackIndex) * kTrackRowHeight;
                    const float y = rowMinY + kTrackRowHeight * 0.5f;
                    const float x = frameToX(targetFrame);
                    drawList->AddRect(
                        ImVec2(x - 7.f, y - 10.f),
                        ImVec2(x + 7.f, y + 10.f),
                        IM_COL32(255, 230, 0, 220),
                        2.f,
                        0,
                        2.f);

                    if (targetFrame != currentKeyFrame)
                    {
                        result.clicked = true;
                        result.hitKey = true;
                        result.draggedKey = true;
                        result.trackIndex = targetTrackIndex;
                        result.keyIndex = targetKeyIndex;
                        result.frame = targetFrame;
                    }
                }
            }
            else
            {
                _draggingKey = false;
                _dragTrackIndex = -1;
                _dragKeyIndex = -1;
            }
        }

        ImGui::EndChild();
        return result;
    }
}
