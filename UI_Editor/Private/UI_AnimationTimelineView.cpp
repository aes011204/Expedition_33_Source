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
        constexpr float kKeyDragThreshold = 4.f;

        struct TimelineRow
        {
            int32 targetIndex = -1;
            int32 trackIndex = -1;
            bool isTargetRow = false;
        };

        vector<TimelineRow> Build_Rows(const UI_AnimationAsset& asset)
        {
            vector<TimelineRow> rows;
            rows.reserve(asset.targets.size() * 2);

            for (int32 targetIndex = 0; targetIndex < static_cast<int32>(asset.targets.size()); ++targetIndex)
            {
                rows.push_back({ targetIndex, -1, true });

                const UI_AnimationTargetBinding& target = asset.targets[targetIndex];
                for (int32 trackIndex = 0; trackIndex < static_cast<int32>(target.tracks.size()); ++trackIndex)
                    rows.push_back({ targetIndex, trackIndex, false });
            }

            return rows;
        }
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
        int32 selectedTargetIndex,
        int32 selectedTrackIndex,
        int32 selectedKeyIndex,
        const ImVec2& size)
    {
        UI_AnimationTimelineClickResult result = {};
        result.frame = currentFrame;

        const vector<TimelineRow> rows = Build_Rows(asset);
        const int32 rowCount = max(1, static_cast<int32>(rows.size()));
        const int32 frameCount = max(1, asset.endFrame - asset.startFrame + 1);
        const float contentWidth = max(size.x, static_cast<float>(frameCount) * kFrameWidth + 32.f);
        const float contentHeight = max(size.y, kRulerHeight + rowCount * kTrackRowHeight + 16.f);

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

        auto yToRowIndex = [&](float y) -> int32
        {
            if (y < origin.y + kRulerHeight)
                return -1;

            const int32 rowIndex = static_cast<int32>((y - origin.y - kRulerHeight) / kTrackRowHeight);
            return rowIndex >= 0 && rowIndex < static_cast<int32>(rows.size()) ? rowIndex : -1;
        };

        auto hitTestKey = [&](const ImVec2& screenPos, int32& outTargetIndex, int32& outTrackIndex, int32& outKeyIndex) -> bool
        {
            outTargetIndex = -1;
            outTrackIndex = -1;
            outKeyIndex = -1;

            const int32 rowIndex = yToRowIndex(screenPos.y);
            if (rowIndex < 0)
                return false;

            const TimelineRow& row = rows[rowIndex];
            if (row.isTargetRow || row.targetIndex < 0 || row.trackIndex < 0)
                return false;

            const UI_AnimationTargetBinding& target = asset.targets[row.targetIndex];
            const UI_AnimationKeyframeTrack& track = target.tracks[row.trackIndex];
            for (int32 keyIndex = 0; keyIndex < static_cast<int32>(track.keys.size()); ++keyIndex)
            {
                const float keyX = frameToX(track.keys[keyIndex].frame);
                if (fabsf(screenPos.x - keyX) <= 7.f)
                {
                    outTargetIndex = row.targetIndex;
                    outTrackIndex = row.trackIndex;
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

        for (int32 rowIndex = 0; rowIndex < static_cast<int32>(rows.size()); ++rowIndex)
        {
            const TimelineRow& row = rows[rowIndex];
            const float rowMinY = origin.y + kRulerHeight + static_cast<float>(rowIndex) * kTrackRowHeight;
            const float rowMaxY = rowMinY + kTrackRowHeight;
            const bool isSelectedTarget = selectedTargetIndex == row.targetIndex;
            const bool isSelectedTrack = isSelectedTarget && !row.isTargetRow && selectedTrackIndex == row.trackIndex;

            const ImU32 rowColor = row.isTargetRow
                ? (isSelectedTarget ? IM_COL32(42, 71, 92, 255) : IM_COL32(34, 54, 68, 255))
                : (isSelectedTrack ? IM_COL32(62, 66, 76, 255) : (rowIndex % 2 == 0 ? IM_COL32(42, 43, 48, 255) : IM_COL32(37, 38, 42, 255)));
            drawList->AddRectFilled(ImVec2(origin.x, rowMinY), ImVec2(canvasMax.x, rowMaxY), rowColor);

            if (row.isTargetRow)
            {
                const UI_AnimationTargetBinding& target = asset.targets[row.targetIndex];
                const string label = target.targetName.empty() ? string("(Unnamed Target)") : target.targetName;
                drawList->AddText(ImVec2(origin.x + 8.f, rowMinY + 9.f), IM_COL32(232, 238, 245, 240), label.c_str());
                continue;
            }

            const UI_AnimationTargetBinding& target = asset.targets[row.targetIndex];
            const UI_AnimationKeyframeTrack& track = target.tracks[row.trackIndex];
            drawList->AddText(
                ImVec2(origin.x + 24.f, rowMinY + 9.f),
                IM_COL32(222, 224, 232, 235),
                Get_AnimationKeyframeTrackTypeName(track.type));

            for (int32 keyIndex = 0; keyIndex < static_cast<int32>(track.keys.size()); ++keyIndex)
            {
                const UI_AnimationKeyframe& key = track.keys[keyIndex];
                const float x = frameToX(key.frame);
                const float y = rowMinY + kTrackRowHeight * 0.5f;
                const bool isSelectedKey = isSelectedTrack && selectedKeyIndex == keyIndex;
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
            _dragStarted = false;
            _dragTargetIndex = -1;
            _dragTrackIndex = -1;
            _dragKeyIndex = -1;
            _dragStartMousePos = mousePos;

            const int32 rowIndex = yToRowIndex(mousePos.y);
            if (rowIndex >= 0)
            {
                const TimelineRow& row = rows[rowIndex];
                result.targetIndex = row.targetIndex;
                result.trackIndex = row.trackIndex;
            }

            if (result.targetIndex >= 0 && result.trackIndex >= 0)
            {
                int32 hitTargetIndex = -1;
                int32 hitTrackIndex = -1;
                int32 hitKeyIndex = -1;
                if (hitTestKey(mousePos, hitTargetIndex, hitTrackIndex, hitKeyIndex))
                {
                    result.hitKey = true;
                    result.targetIndex = hitTargetIndex;
                    result.trackIndex = hitTrackIndex;
                    result.keyIndex = hitKeyIndex;
                    result.frame = asset.targets[hitTargetIndex].tracks[hitTrackIndex].keys[hitKeyIndex].frame;

                    _draggingKey = true;
                    _dragStarted = false;
                    _dragTargetIndex = hitTargetIndex;
                    _dragTrackIndex = hitTrackIndex;
                    _dragKeyIndex = hitKeyIndex;
                    _dragStartMousePos = mousePos;
                }
            }
        }

        if (_draggingKey)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                const float dragDeltaX = mousePos.x - _dragStartMousePos.x;
                const float dragDeltaY = mousePos.y - _dragStartMousePos.y;
                const float dragDistanceSq = dragDeltaX * dragDeltaX + dragDeltaY * dragDeltaY;
                if (!_dragStarted && dragDistanceSq >= kKeyDragThreshold * kKeyDragThreshold)
                    _dragStarted = true;

                const bool useSelectedKey =
                    _dragStarted &&
                    selectedTargetIndex == _dragTargetIndex &&
                    selectedTrackIndex == _dragTrackIndex &&
                    selectedKeyIndex >= 0;
                const int32 targetTargetIndex = useSelectedKey ? selectedTargetIndex : _dragTargetIndex;
                const int32 targetTrackIndex = useSelectedKey ? selectedTrackIndex : _dragTrackIndex;
                const int32 targetKeyIndex = useSelectedKey ? selectedKeyIndex : _dragKeyIndex;
                const int32 targetFrame = xToFrame(mousePos.x);

                if (targetTargetIndex >= 0 &&
                    targetTargetIndex < static_cast<int32>(asset.targets.size()) &&
                    targetTrackIndex >= 0 &&
                    targetTrackIndex < static_cast<int32>(asset.targets[targetTargetIndex].tracks.size()) &&
                    targetKeyIndex >= 0 &&
                    targetKeyIndex < static_cast<int32>(asset.targets[targetTargetIndex].tracks[targetTrackIndex].keys.size()))
                {
                    const int32 currentKeyFrame = asset.targets[targetTargetIndex].tracks[targetTrackIndex].keys[targetKeyIndex].frame;
                    int32 rowIndex = -1;
                    for (int32 index = 0; index < static_cast<int32>(rows.size()); ++index)
                    {
                        if (rows[index].targetIndex == targetTargetIndex && rows[index].trackIndex == targetTrackIndex)
                        {
                            rowIndex = index;
                            break;
                        }
                    }

                    if (rowIndex >= 0)
                    {
                        const float rowMinY = origin.y + kRulerHeight + static_cast<float>(rowIndex) * kTrackRowHeight;
                        const float y = rowMinY + kTrackRowHeight * 0.5f;
                        const float x = frameToX(targetFrame);
                        drawList->AddRect(
                            ImVec2(x - 7.f, y - 10.f),
                            ImVec2(x + 7.f, y + 10.f),
                            IM_COL32(255, 230, 0, 220),
                            2.f,
                            0,
                            2.f);
                    }

                    if (_dragStarted && targetFrame != currentKeyFrame)
                    {
                        result.clicked = true;
                        result.hitKey = true;
                        result.draggedKey = true;
                        result.targetIndex = targetTargetIndex;
                        result.trackIndex = targetTrackIndex;
                        result.keyIndex = targetKeyIndex;
                        result.frame = targetFrame;
                    }
                }
            }
            else
            {
                _draggingKey = false;
                _dragStarted = false;
                _dragTargetIndex = -1;
                _dragTrackIndex = -1;
                _dragKeyIndex = -1;
                _dragStartMousePos = ImVec2(0.f, 0.f);
            }
        }

        ImGui::EndChild();
        return result;
    }
}
