#include "AnimSequencerAdapter.h"
#include "Animation_View.h"
#include "AnimNotify_Parser.h"
#include "ModelCom.h"
#include "AnimNotify.h"
#include "AnimNotifyState.h"
#include <algorithm>
#include <cmath>

NS_BEGIN(MainEditor)

namespace
{
    constexpr int32 kSequencerNotifyHeaderRow = 0;

    int32 Get_NotifyTrackCount(const AnimNotifyData* animationData)
    {
        return animationData ? max(1, static_cast<int32>(animationData->notifyTracks.size())) : 0;
    }

    int32 Get_StateHeaderRow(const AnimNotifyData* animationData)
    {
        return kSequencerNotifyHeaderRow + 1 + Get_NotifyTrackCount(animationData);
    }

    int32 Get_StateTrackCount(const AnimNotifyData* animationData)
    {
        return animationData ? max(1, static_cast<int32>(animationData->notifyStateTracks.size())) : 0;
    }

    string Normalize_DefaultTrackName(const string& trackName)
    {
        if (trackName == "노티파이")
            return "Notify";
        if (trackName == "노티파이 스테이트")
            return "Notify State";
        if (trackName == "노티파이 트랙")
            return "Notify Track";
        if (trackName == "노티파이 스테이트 트랙")
            return "Notify State Track";

        return trackName;
    }

    string Make_SequencerRowLabel(const string& trackName, bool isStateTrack)
    {
        const string normalized = Normalize_DefaultTrackName(trackName);

        if (normalized.empty())
            return isStateTrack ? "Notify States" : "Notifies";

        if (normalized == "Notify" || normalized == "Notify Track")
            return "Notifies";

        if (normalized == "Notify State" || normalized == "Notify State Track")
            return "Notify States";

        return normalized;
    }

    int32 Resolve_TrackIndexFromMouseY(int32 currentTrackIndex, int32 trackCount, const ImRect& currentRowRect, float mouseY)
    {
        if (trackCount <= 1)
            return 0;

        const float rowHeight = max(1.f, currentRowRect.Max.y - currentRowRect.Min.y);
        const float firstTrackTop = currentRowRect.Min.y - static_cast<float>(currentTrackIndex) * rowHeight;
        const int32 hoveredTrackIndex = static_cast<int32>(std::floor((mouseY - firstTrackTop) / rowHeight));
        return std::clamp(hoveredTrackIndex, 0, trackCount - 1);
    }

}

AnimSequencerAdapter::AnimSequencerAdapter(Animation_View* view)
    : _view(view)
{
}

int AnimSequencerAdapter::GetFrameMin() const
{
    return 0;
}

int AnimSequencerAdapter::GetFrameMax() const
{
    if (_view == nullptr)
        return 1;

    return max(1, _view->Get_CurrentAnimationFrameMax());
}

int AnimSequencerAdapter::GetViewFrameMin() const
{
    if (_view == nullptr)
        return GetFrameMin();

    const int32 frameMin = GetFrameMin();
    const int32 frameMax = GetFrameMax();
    const int32 fullRange = max(1, frameMax - frameMin);
    const int32 visibleRange = _view->_sequencerVisibleFrameCount <= 0
        ? fullRange
        : std::clamp(_view->_sequencerVisibleFrameCount, 1, fullRange);

    return std::clamp(_view->_sequencerFirstFrame, frameMin, max(frameMin, frameMax - visibleRange));
}

int AnimSequencerAdapter::GetViewFrameMax() const
{
    const int32 frameMax = GetFrameMax();
    const int32 viewFrameMin = GetViewFrameMin();
    const int32 fullRange = max(1, frameMax - GetFrameMin());
    const int32 visibleRange = _view == nullptr || _view->_sequencerVisibleFrameCount <= 0
        ? fullRange
        : std::clamp(_view->_sequencerVisibleFrameCount, 1, fullRange);

    return std::clamp(viewFrameMin + visibleRange, viewFrameMin + 1, frameMax);
}

int AnimSequencerAdapter::GetItemCount() const
{
    const AnimNotifyData* animationData = _view ? _view->Get_CurrentAnimation() : nullptr;
    if (animationData == nullptr)
        return 0;

    return 2 + Get_NotifyTrackCount(animationData) + Get_StateTrackCount(animationData);
}

bool AnimSequencerAdapter::Resolve_TrackRow(int32 rowIndex, bool& outIsStateTrack, int32& outTrackIndex) const
{
    const AnimNotifyData* animationData = _view ? _view->Get_CurrentAnimation() : nullptr;
    if (animationData == nullptr || rowIndex < 0)
        return false;

    const int32 notifyTrackCount = Get_NotifyTrackCount(animationData);
    const int32 stateHeaderRow = Get_StateHeaderRow(animationData);
    const int32 stateTrackCount = Get_StateTrackCount(animationData);

    if (rowIndex == kSequencerNotifyHeaderRow)
    {
        outIsStateTrack = false;
        outTrackIndex = 0;
        return true;
    }

    if (rowIndex > kSequencerNotifyHeaderRow && rowIndex < stateHeaderRow)
    {
        outIsStateTrack = false;
        outTrackIndex = std::clamp(rowIndex - 1, 0, notifyTrackCount - 1);
        return true;
    }

    if (rowIndex == stateHeaderRow)
    {
        outIsStateTrack = true;
        outTrackIndex = 0;
        return true;
    }

    if (rowIndex > stateHeaderRow && rowIndex <= stateHeaderRow + stateTrackCount)
    {
        outIsStateTrack = true;
        outTrackIndex = std::clamp(rowIndex - stateHeaderRow - 1, 0, stateTrackCount - 1);
        return true;
    }

    return false;
}

const char* AnimSequencerAdapter::GetItemLabel(int index) const
{
    const AnimNotifyData* animationData = _view ? _view->Get_CurrentAnimation() : nullptr;
    if (animationData == nullptr)
        return "";

    bool isStateTrack = false;
    int32 trackIndex = 0;
    if (!Resolve_TrackRow(index, isStateTrack, trackIndex))
        return "";

    if (_rowLabels.size() <= static_cast<size_t>(index))
        _rowLabels.resize(index + 1);

    if (index == kSequencerNotifyHeaderRow)
        _rowLabels[index] = "Notifies";
    else if (index == Get_StateHeaderRow(animationData))
        _rowLabels[index] = "Notify States";
    else
    {
        const vector<AnimNotifyTrackDesc>& tracks = isStateTrack
            ? animationData->notifyStateTracks
            : animationData->notifyTracks;

        if (trackIndex >= 0 && trackIndex < static_cast<int32>(tracks.size()))
            _rowLabels[index] = Normalize_DefaultTrackName(tracks[trackIndex].name);
        else
            _rowLabels[index] = isStateTrack ? "Notify State" : "Notify";

        if (_rowLabels[index].empty())
            _rowLabels[index] = isStateTrack ? "Notify State" : "Notify";
    }

    return _rowLabels[index].c_str();
}

void AnimSequencerAdapter::Get(int index, int** start, int** end, int* type, unsigned* color)
{
    bool isStateTrack = false;
    int32 trackIndex = 0;
    if (!Resolve_TrackRow(index, isStateTrack, trackIndex))
        return;

    if (_rowStart.size() <= static_cast<size_t>(index))
        _rowStart.resize(index + 1);

    if (_rowEnd.size() <= static_cast<size_t>(index))
        _rowEnd.resize(index + 1);

    _rowStart[index] = GetFrameMin();
    _rowEnd[index] = GetFrameMax();

    if (start)
        *start = &_rowStart[index];
    if (end)
        *end = &_rowEnd[index];
    if (type)
        *type = index;
    if (color)
    {
        const int32 stateHeaderRow = Get_StateHeaderRow(_view ? _view->Get_CurrentAnimation() : nullptr);

        if (index == kSequencerNotifyHeaderRow)
            *color = TrackColors::NotifyFill;
        else if (index > kSequencerNotifyHeaderRow && index < stateHeaderRow)
            *color = TrackColors::NotifyTrack;
        else if (index == stateHeaderRow)
            *color = TrackColors::StateFill;
        else if (index > stateHeaderRow)
            *color = TrackColors::StateTrack;
        else
            *color = isStateTrack ? TrackColors::StateFill : TrackColors::NotifyFill;
    }
}

void AnimSequencerAdapter::CustomDraw(int index, ImDrawList* drawList, const ImRect& rc, const ImRect& legendRect,
    const ImRect& clippingRect, const ImRect& legendClippingRect)
{
    SequenceInterface::CustomDraw(index, drawList, rc, legendRect, clippingRect, legendClippingRect);

    if (_view == nullptr)
        return;

    bool isStateTrack = false;
    int32 trackIndex = 0;
    if (!Resolve_TrackRow(index, isStateTrack, trackIndex))
        return;

    const AnimNotifyData* animationData = _view->Get_CurrentAnimation();
    const int32 stateHeaderRow = Get_StateHeaderRow(animationData);

    if (index > kSequencerNotifyHeaderRow && index < stateHeaderRow)
    {
        Draw_NotifyRow(trackIndex, drawList, rc);
        return;
    }

    if (index > stateHeaderRow)
    {
        Draw_StateRow(trackIndex, drawList, rc);
        Handle_StateMarkGesture(trackIndex, rc);
    }
}

void AnimSequencerAdapter::BeginEdit(int index)
{
}

void AnimSequencerAdapter::EndEdit()
{
    if (_view)
        _view->MarkDirty();
}

int32 AnimSequencerAdapter::TimeSec_ToFrame(float timeSec, int32 fps)
{
    return static_cast<int32>(std::round(timeSec * static_cast<float>(max(1, fps))));
}

float AnimSequencerAdapter::Frame_ToTimeSec(int32 frame, int32 fps)
{
    return static_cast<float>(frame) / static_cast<float>(max(1, fps));
}

void AnimSequencerAdapter::Draw_NotifyRow(int32 trackIndex, ImDrawList* drawList, const ImRect& rc)
{
    if (_view == nullptr)
        return;

    AnimNotifyData* animationData = _view->Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    const int32 fps = max(1, _view->Get_CurrentAnimationFps());
    const float animationLengthSec = _view->Get_CurrentAnimationLengthSec();
    const float width = rc.Max.x - rc.Min.x;
    const int32 viewFrameMin = GetViewFrameMin();
    const int32 viewFrameMax = max(viewFrameMin + 1, GetViewFrameMax());
    const float pixelsPerFrame = width / static_cast<float>(max(1, viewFrameMax - viewFrameMin));
    const bool isSelectedTrack = (_view->_selectedNotifyTrackIndex == trackIndex);

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mousePos = io.MousePos;
    const bool rowHovered = rc.Contains(mousePos);
    const bool isLeftDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    bool clickedAnyItem = false;

    if (isSelectedTrack)
    {
        drawList->AddRectFilled(
            ImVec2(rc.Min.x, rc.Min.y + 1.f),
            ImVec2(rc.Max.x, rc.Max.y - 1.f),
            TrackColors::NotifyTrack,
            3.f);
    }

    drawList->PushClipRect(rc.Min, rc.Max, true);

    for (auto& entry : animationData->notifies)
    {
        if (entry.trackIndex != trackIndex)
            continue;

        const int32 frame = TimeSec_ToFrame(entry.timeSec, fps);
        if (frame < viewFrameMin || frame > viewFrameMax)
            continue;

        const float x = rc.Min.x + static_cast<float>(frame - viewFrameMin) * pixelsPerFrame;
        const float markerWidth = min(88.f, max(12.f, width));
        const float markerHeight = (rc.Max.y - rc.Min.y) - 6.f;
        const bool anchorRight = (x + markerWidth > rc.Max.x);
        const float markerMinX = std::clamp(
            anchorRight ? (x - markerWidth) : x,
            rc.Min.x,
            max(rc.Min.x, rc.Max.x - markerWidth));

        ImRect itemRect(
            ImVec2(markerMinX, rc.Min.y + 3.f),
            ImVec2(markerMinX + markerWidth, rc.Min.y + 3.f + markerHeight));

        const float anchorHandleWidth = 7.f;
        ImRect anchorHandleRect = anchorRight
            ? ImRect(
                ImVec2(itemRect.Max.x - anchorHandleWidth, itemRect.Min.y),
                ImVec2(itemRect.Max.x, itemRect.Max.y))
            : ImRect(
                ImVec2(itemRect.Min.x, itemRect.Min.y),
                ImVec2(itemRect.Min.x + anchorHandleWidth, itemRect.Max.y));

        const bool selected = _view->Is_NotifySelected(entry.id);
        const bool hovered = itemRect.Contains(mousePos);

        const ImU32 fillColor = hovered
            ? TrackColors::NotifyHover
            : TrackColors::NotifyFill;

        const ImU32 borderColor = selected
            ? TrackColors::SelectedBorder
            : TrackColors::NotifyBorder;
        const ImU32 anchorColor = selected
            ? TrackColors::SelectedBorder
            : IM_COL32(255, 244, 120, 255);

        drawList->AddRectFilled(itemRect.Min, itemRect.Max, fillColor, 3.f);
        drawList->AddRect(itemRect.Min, itemRect.Max, borderColor, 3.f, 0, selected ? 2.f : 1.0f);
        drawList->AddRectFilled(anchorHandleRect.Min, anchorHandleRect.Max, anchorColor, 2.f);

        const string typeName = entry.notify ? entry.notify->Get_TypeName() : string{};
        if (!typeName.empty())
        {
            const ImVec2 textSize = ImGui::CalcTextSize(typeName.c_str());
            const float textX = itemRect.Min.x + ((itemRect.Max.x - itemRect.Min.x) - textSize.x) * 0.5f;
            const float textY = itemRect.Min.y + ((itemRect.Max.y - itemRect.Min.y) - textSize.y) * 0.5f;
            drawList->AddText(
                ImVec2(textX, textY),
                TrackColors::Text,
                typeName.c_str());
        }

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            clickedAnyItem = true;
            const bool ctrlPressed = io.KeyCtrl;
            const bool keepExistingSelection = !ctrlPressed && selected && _view->_selectedNotifyIds.size() > 1;
            _view->Begin_PendingNotifyUndo();

            if (ctrlPressed)
            {
                if (_view->Is_NotifySelected(entry.id))
                {
                    _view->_selectedNotifyIds.erase(entry.id);
                    if (_view->_selectedNotifyId == entry.id)
                        _view->_selectedNotifyId = _view->_selectedNotifyIds.empty() ? -1 : *_view->_selectedNotifyIds.begin();
                }
                else
                {
                    _view->_selectedNotifyIds.insert(entry.id);
                    _view->_selectedNotifyId = entry.id;
                }
            }
            else if (keepExistingSelection)
            {
                _view->_selectedNotifyId = entry.id;
            }
            else
            {
                _view->_selectedNotifyIds.clear();
                _view->_selectedNotifyIds.insert(entry.id);
                _view->_selectedNotifyId = entry.id;
            }

            _view->Clear_SelectedStateSelection();
            _view->_selectedNotifyTrackIndex = trackIndex;
            _view->_sequencerSelectedRow = kSequencerNotifyHeaderRow + 1 + trackIndex;
            _draggingNotifyId = entry.id;
            _notifyDragStartFrame = frame;
            _notifyDragStartTrackIndex = trackIndex;
            _notifyDragEntries.clear();
            _notifyDragEntries.reserve(_view->_selectedNotifyIds.size());
            for (const AnimNotifyEventEntry& dragEntry : animationData->notifies)
            {
                if (!_view->Is_NotifySelected(dragEntry.id))
                    continue;

                const int32 dragStartFrame = std::clamp(
                    TimeSec_ToFrame(dragEntry.timeSec, fps),
                    GetFrameMin(),
                    max(GetFrameMin(), GetFrameMax() - 1));
                _notifyDragEntries.push_back({ dragEntry.id, dragStartFrame, dragEntry.trackIndex });
            }
        }

        if (_draggingNotifyId == entry.id && isLeftDragging)
        {
            const float clampedX = std::clamp(mousePos.x, rc.Min.x, rc.Max.x);
            const float relative = clampedX - rc.Min.x;
            const int32 draggedFrame = viewFrameMin + static_cast<int32>(std::round(relative / max(pixelsPerFrame, 0.0001f)));
            const int32 maxNotifyFrame = max(GetFrameMin(), GetFrameMax() - 1);
            const int32 clampedFrame = std::clamp(draggedFrame, GetFrameMin(), maxNotifyFrame);
            const int32 targetTrackIndex = Resolve_TrackIndexFromMouseY(
                trackIndex,
                Get_NotifyTrackCount(animationData),
                rc,
                mousePos.y);
            const int32 requestedFrameDelta = clampedFrame - _notifyDragStartFrame;
            const int32 requestedTrackDelta = targetTrackIndex - _notifyDragStartTrackIndex;
            int32 minFrameDelta = GetFrameMin() - _notifyDragStartFrame;
            int32 maxFrameDelta = maxNotifyFrame - _notifyDragStartFrame;
            const int32 maxTrackIndex = max(0, Get_NotifyTrackCount(animationData) - 1);

            for (const NotifyDragEntry& dragEntry : _notifyDragEntries)
            {
                minFrameDelta = max(minFrameDelta, GetFrameMin() - dragEntry.startFrame);
                maxFrameDelta = min(maxFrameDelta, maxNotifyFrame - dragEntry.startFrame);
            }

            const int32 appliedFrameDelta = std::clamp(requestedFrameDelta, minFrameDelta, maxFrameDelta);
            float primaryTimeSec = Frame_ToTimeSec(clampedFrame, fps);

            for (AnimNotifyEventEntry& moveEntry : animationData->notifies)
            {
                const auto dragIt = std::find_if(
                    _notifyDragEntries.begin(),
                    _notifyDragEntries.end(),
                    [&moveEntry](const NotifyDragEntry& dragEntry)
                    {
                        return dragEntry.id == moveEntry.id;
                    });

                if (dragIt == _notifyDragEntries.end())
                    continue;

                const int32 movedFrame = dragIt->startFrame + appliedFrameDelta;
                const int32 movedTrackIndex = std::clamp(dragIt->startTrackIndex + requestedTrackDelta, 0, maxTrackIndex);
                moveEntry.timeSec = std::clamp(
                    Frame_ToTimeSec(movedFrame, fps),
                    0.f,
                    animationLengthSec);
                moveEntry.trackIndex = movedTrackIndex;

                if (moveEntry.id == _draggingNotifyId)
                    primaryTimeSec = moveEntry.timeSec;
            }

            _view->_sequencerCurrentFrame = std::clamp(
                TimeSec_ToFrame(primaryTimeSec, fps),
                GetFrameMin(),
                maxNotifyFrame);
            _view->_selectedNotifyTrackIndex = targetTrackIndex;
            _view->_sequencerSelectedRow = kSequencerNotifyHeaderRow + 1 + targetTrackIndex;
            _view->_isDirty = true;

            Shared<ModelCom> playbackModel = _view->Get_PlaybackModel();
            if (_view->_sourceMode == Animation_View::SourceMode::LiveModel && playbackModel != nullptr)
            {
                playbackModel->Set_AnimationPaused(true);
                playbackModel->Set_CurrentAnimationTimeSec(primaryTimeSec);
                playbackModel->Refresh_CurrentAnimationPose();
            }
        }
    }

    drawList->PopClipRect();

    if (_draggingNotifyId != -1 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        AnimNotify_Parser::Sort_And_Clamp(*animationData, animationLengthSec);
        _draggingNotifyId = -1;
        _notifyDragEntries.clear();
        _view->Commit_PendingNotifyUndo();
    }

    if (!clickedAnyItem && rowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        _view->Clear_SelectedNotifySelection();
        _view->Clear_SelectedStateSelection();
        _view->_selectedNotifyTrackIndex = trackIndex;
        _view->_sequencerSelectedRow = kSequencerNotifyHeaderRow + 1 + trackIndex;
    }
}

void AnimSequencerAdapter::Draw_StateRow(int32 trackIndex, ImDrawList* drawList, const ImRect& rc)
{
    if (_view == nullptr)
        return;

    AnimNotifyData* animationData = _view->Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    const int32 fps = max(1, _view->Get_CurrentAnimationFps());
    const float animationLengthSec = _view->Get_CurrentAnimationLengthSec();
    const float width = rc.Max.x - rc.Min.x;
    const int32 viewFrameMin = GetViewFrameMin();
    const int32 viewFrameMax = max(viewFrameMin + 1, GetViewFrameMax());
    const float pixelsPerFrame = width / static_cast<float>(max(1, viewFrameMax - viewFrameMin));
    const bool isSelectedTrack = (_view->_selectedNotifyStateTrackIndex == trackIndex);

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mousePos = io.MousePos;
    const bool rowHovered = rc.Contains(mousePos);
    const bool isLeftDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    bool clickedAnyItem = false;

    if (isSelectedTrack)
    {
        drawList->AddRectFilled(
            ImVec2(rc.Min.x, rc.Min.y + 1.f),
            ImVec2(rc.Max.x, rc.Max.y - 1.f),
            TrackColors::StateTrack,
            3.f);
    }

    drawList->PushClipRect(rc.Min, rc.Max, true);

    for (auto& entry : animationData->notifyStates)
    {
        if (entry.trackIndex != trackIndex)
            continue;

        const int32 startFrame = TimeSec_ToFrame(entry.startSec, fps);
        const int32 endFrame = max(startFrame + 1, TimeSec_ToFrame(entry.startSec + entry.durationSec, fps));
        if (endFrame <= viewFrameMin || startFrame >= viewFrameMax)
            continue;

        const string typeName = entry.notifyState ? entry.notifyState->Get_TypeName() : string{};
        const float startX = rc.Min.x + static_cast<float>(startFrame - viewFrameMin) * pixelsPerFrame;
        const float endX = rc.Min.x + static_cast<float>(endFrame - viewFrameMin) * pixelsPerFrame;
        const float visualMinWidth = max(2.f, pixelsPerFrame);
        const float visualEndX = max(endX, startX + visualMinWidth);
        const float drawStartX = std::clamp(startX, rc.Min.x, rc.Max.x);
        const float drawEndX = std::clamp(visualEndX, rc.Min.x, rc.Max.x);

        ImRect itemRect(
            ImVec2(drawStartX, rc.Min.y + 4.f),
            ImVec2(max(drawStartX + visualMinWidth, drawEndX), rc.Max.y - 4.f));
        itemRect.Max.x = min(itemRect.Max.x, rc.Max.x);

        ImRect interactionRect = itemRect;
        const float interactionMinWidth = 18.f;
        if ((interactionRect.Max.x - interactionRect.Min.x) < interactionMinWidth)
        {
            const float expand = (interactionMinWidth - (interactionRect.Max.x - interactionRect.Min.x)) * 0.5f;
            interactionRect.Min.x -= expand;
            interactionRect.Max.x += expand;
        }
        interactionRect.Min.x = max(interactionRect.Min.x, rc.Min.x);
        interactionRect.Max.x = min(interactionRect.Max.x, rc.Max.x);

        const bool selected = _view->Is_StateSelected(entry.id);
        const bool hovered = interactionRect.Contains(mousePos);

        const float drawHandleWidth = min(10.f, max(4.f, (itemRect.Max.x - itemRect.Min.x) * 0.35f));
        const float hitHandleWidth = max(drawHandleWidth, 10.f);
        const bool canDrawLeftHandle = startFrame >= viewFrameMin;
        const bool canDrawRightHandle = endFrame <= viewFrameMax;
        ImRect leftHandleDraw(
            ImVec2(itemRect.Min.x, itemRect.Min.y),
            ImVec2(itemRect.Min.x + drawHandleWidth, itemRect.Max.y));

        ImRect rightHandleDraw(
            ImVec2(itemRect.Max.x - drawHandleWidth, itemRect.Min.y),
            ImVec2(itemRect.Max.x, itemRect.Max.y));

        ImRect leftHandleHit(
            ImVec2(interactionRect.Min.x, interactionRect.Min.y),
            ImVec2(interactionRect.Min.x + hitHandleWidth, interactionRect.Max.y));

        ImRect rightHandleHit(
            ImVec2(interactionRect.Max.x - hitHandleWidth, interactionRect.Min.y),
            ImVec2(interactionRect.Max.x, interactionRect.Max.y));

        const bool hoverLeft = canDrawLeftHandle && leftHandleHit.Contains(mousePos);
        const bool hoverRight = canDrawRightHandle && rightHandleHit.Contains(mousePos);

        const ImU32 fillColor = hovered
            ? TrackColors::StateHover
            : TrackColors::StateFill;

        const ImU32 borderColor = selected
            ? TrackColors::SelectedBorder
            : TrackColors::StateBorder;

        drawList->AddRectFilled(itemRect.Min, itemRect.Max, fillColor, 3.f);
        drawList->AddRect(itemRect.Min, itemRect.Max, borderColor, 3.f, 0, selected ? 2.5f : 1.0f);
        if (canDrawLeftHandle)
            drawList->AddRectFilled(leftHandleDraw.Min, leftHandleDraw.Max, hoverLeft ? TrackColors::StateHandleHover : TrackColors::StateHandle);
        if (canDrawRightHandle)
            drawList->AddRectFilled(rightHandleDraw.Min, rightHandleDraw.Max, hoverRight ? TrackColors::StateHandleHover : TrackColors::StateHandle);

        if (!typeName.empty())
        {
            const ImVec2 textSize = ImGui::CalcTextSize(typeName.c_str());
            const float availableTextWidth = (itemRect.Max.x - itemRect.Min.x) - (drawHandleWidth * 2.f) - 8.f;
            const float textY = itemRect.Min.y + ((itemRect.Max.y - itemRect.Min.y) - textSize.y) * 0.5f;
            if (availableTextWidth >= textSize.x)
            {
                drawList->AddText(
                    ImVec2(itemRect.Min.x + drawHandleWidth + 4.f, textY),
                    TrackColors::Text,
                    typeName.c_str());
            }
            else
            {
                const float labelX = min(itemRect.Max.x + 6.f, rc.Max.x - textSize.x - 2.f);
                drawList->AddText(
                    ImVec2(max(labelX, rc.Min.x + 2.f), textY),
                    TrackColors::Text,
                    typeName.c_str());
            }
        }

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            clickedAnyItem = true;
            _view->Begin_PendingNotifyUndo();
            const bool ctrlPressed = io.KeyCtrl;

            if (ctrlPressed)
            {
                if (_view->Is_StateSelected(entry.id))
                {
                    _view->_selectedStateIds.erase(entry.id);
                    if (_view->_selectedStateId == entry.id)
                        _view->_selectedStateId = _view->_selectedStateIds.empty() ? -1 : *_view->_selectedStateIds.begin();
                }
                else
                {
                    _view->_selectedStateIds.insert(entry.id);
                    _view->_selectedStateId = entry.id;
                }
            }
            else
            {
                _view->_selectedStateIds.clear();
                _view->_selectedStateIds.insert(entry.id);
                _view->_selectedStateId = entry.id;
            }

            _view->Clear_SelectedNotifySelection();
            _view->_selectedNotifyStateTrackIndex = trackIndex;
            _view->_sequencerSelectedRow = Get_StateHeaderRow(animationData) + 1 + trackIndex;

            _draggingStateId = entry.id;
            _stateDragMode = hoverLeft ? EStateDragMode::ResizeStart :
                (hoverRight ? EStateDragMode::ResizeEnd : EStateDragMode::Move);
            _stateDragOffsetX = mousePos.x - itemRect.Min.x;
        }

        if (_draggingStateId == entry.id && isLeftDragging)
        {
            const int32 currentStartFrame = TimeSec_ToFrame(entry.startSec, fps);
            const int32 currentDurationFrame = max(1, TimeSec_ToFrame(entry.durationSec, fps));
            const int32 currentEndFrame = currentStartFrame + currentDurationFrame;

            if (_stateDragMode == EStateDragMode::ResizeStart)
            {
                const float clampedX = std::clamp(mousePos.x, rc.Min.x, rc.Max.x);
                const float relative = clampedX - rc.Min.x;
                const int32 draggedFrame = viewFrameMin + static_cast<int32>(std::round(relative / max(pixelsPerFrame, 0.0001f)));
                const int32 newStartFrame = std::clamp(draggedFrame, GetFrameMin(), currentEndFrame - 1);
                entry.startSec = Frame_ToTimeSec(newStartFrame, fps);
                entry.durationSec = Frame_ToTimeSec(currentEndFrame - newStartFrame, fps);
            }
            else if (_stateDragMode == EStateDragMode::ResizeEnd)
            {
                const float clampedX = std::clamp(mousePos.x, rc.Min.x, rc.Max.x);
                const float relative = clampedX - rc.Min.x;
                const int32 draggedFrame = viewFrameMin + static_cast<int32>(std::round(relative / max(pixelsPerFrame, 0.0001f)));
                const int32 newEndFrame = std::clamp(draggedFrame, currentStartFrame + 1, GetFrameMax());
                entry.durationSec = Frame_ToTimeSec(newEndFrame - currentStartFrame, fps);
            }
            else
            {
                const float localX = std::clamp(mousePos.x - _stateDragOffsetX, rc.Min.x, rc.Max.x);
                const float relative = localX - rc.Min.x;
                const int32 draggedFrame = viewFrameMin + static_cast<int32>(std::round(relative / max(pixelsPerFrame, 0.0001f)));
                const int32 newStartFrame = std::clamp(draggedFrame, GetFrameMin(), max(GetFrameMin(), GetFrameMax() - currentDurationFrame));
                entry.startSec = Frame_ToTimeSec(newStartFrame, fps);
            }

            if (_stateDragMode == EStateDragMode::Move)
            {
                const int32 targetTrackIndex = Resolve_TrackIndexFromMouseY(
                    trackIndex,
                    Get_StateTrackCount(animationData),
                    rc,
                    mousePos.y);
                entry.trackIndex = targetTrackIndex;
                _view->_selectedNotifyStateTrackIndex = targetTrackIndex;
                _view->_sequencerSelectedRow = Get_StateHeaderRow(animationData) + 1 + targetTrackIndex;
            }

            _view->_sequencerCurrentFrame = TimeSec_ToFrame(entry.startSec, fps);
            _view->_isDirty = true;

            Shared<ModelCom> playbackModel = _view->Get_PlaybackModel();
            if (_view->_sourceMode == Animation_View::SourceMode::LiveModel && playbackModel != nullptr)
            {
                playbackModel->Set_AnimationPaused(true);
                playbackModel->Set_CurrentAnimationTimeSec(entry.startSec);
                playbackModel->Refresh_CurrentAnimationPose();
            }
        }
    }

    if (!clickedAnyItem && rowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        _view->Clear_SelectedStateSelection();
        _view->Clear_SelectedNotifySelection();
        _view->_selectedNotifyStateTrackIndex = trackIndex;
        _view->_sequencerSelectedRow = Get_StateHeaderRow(animationData) + 1 + trackIndex;
    }

    if (_view->_sequencerMarkingState &&
        _view->_sequencerContextIsStateTrack &&
        _view->_sequencerContextTrackIndex == trackIndex)
    {
        const int32 startFrame = min(_view->_sequencerMarkStartFrame, _view->_sequencerMarkEndFrame);
        const int32 endFrame = max(_view->_sequencerMarkStartFrame, _view->_sequencerMarkEndFrame);
        const float startX = std::clamp(
            rc.Min.x + static_cast<float>(startFrame - viewFrameMin) * pixelsPerFrame,
            rc.Min.x,
            rc.Max.x);
        const float endX = std::clamp(
            rc.Min.x + static_cast<float>(endFrame - viewFrameMin) * pixelsPerFrame,
            rc.Min.x,
            rc.Max.x);

        drawList->AddRectFilled(
            ImVec2(startX, rc.Min.y + 3.f),
            ImVec2(endX, rc.Max.y - 3.f),
            IM_COL32(136, 187, 255, 85),
            3.f);

        drawList->AddRect(
            ImVec2(startX, rc.Min.y + 3.f),
            ImVec2(endX, rc.Max.y - 3.f),
            IM_COL32(0, 255, 255, 255),
            3.f,
            0,
            2.f);
    }

    drawList->PopClipRect();

    if (_draggingStateId != -1 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        AnimNotify_Parser::Sort_And_Clamp(*animationData, animationLengthSec);
        _draggingStateId = -1;
        _stateDragMode = EStateDragMode::None;
        _stateDragOffsetX = 0.f;
        _view->Commit_PendingNotifyUndo();
    }
}

void AnimSequencerAdapter::Handle_StateMarkGesture(int32 trackIndex, const ImRect& rc)
{
    if (_view == nullptr)
        return;

    ImGuiIO& io = ImGui::GetIO();
    if (!rc.Contains(io.MousePos))
        return;

    if (_draggingNotifyId != -1 || _draggingStateId != -1)
        return;

    const float width = rc.Max.x - rc.Min.x;
    const int32 viewFrameMin = GetViewFrameMin();
    const int32 viewFrameMax = max(viewFrameMin + 1, GetViewFrameMax());
    const float pixelsPerFrame = width / static_cast<float>(max(1, viewFrameMax - viewFrameMin));

    if (!_view->_sequencerMarkingState &&
        io.KeyShift &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        _view->_sequencerMarkingState = true;
        _view->_sequencerContextIsStateTrack = true;
        _view->_sequencerContextTrackIndex = trackIndex;
        _view->_sequencerContextHasTrack = true;
        const float clampedX = std::clamp(io.MousePos.x, rc.Min.x, rc.Max.x);
        const float relative = clampedX - rc.Min.x;
        const int32 startFrame = viewFrameMin + static_cast<int32>(std::round(relative / max(pixelsPerFrame, 0.0001f)));
        _view->_sequencerMarkStartFrame = startFrame;
        _view->_sequencerMarkEndFrame = startFrame;
        return;
    }

    if (_view->_sequencerMarkingState &&
        _view->_sequencerContextIsStateTrack &&
        _view->_sequencerContextTrackIndex == trackIndex &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const float clampedX = std::clamp(io.MousePos.x, rc.Min.x, rc.Max.x);
        const float relative = clampedX - rc.Min.x;
        _view->_sequencerMarkEndFrame = viewFrameMin + static_cast<int32>(std::round(relative / max(pixelsPerFrame, 0.0001f)));
        return;
    }

    if (_view->_sequencerMarkingState &&
        _view->_sequencerContextIsStateTrack &&
        _view->_sequencerContextTrackIndex == trackIndex &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        const float clampedX = std::clamp(io.MousePos.x, rc.Min.x, rc.Max.x);
        const float relative = clampedX - rc.Min.x;
        _view->_sequencerMarkEndFrame = viewFrameMin + static_cast<int32>(std::round(relative / max(pixelsPerFrame, 0.0001f)));
        _view->_sequencerMarkingState = false;
    }
}

NS_END
