#include "AnimSequencerAdapter.h"
#include "Animation_View.h"
#include "AnimNotify_Parser.h"
#include "ModelCom.h"
#include "AnimNotify.h"
#include "AnimNotifyState.h"

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
        _rowLabels[index].clear();

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
    const float pixelsPerFrame = width / static_cast<float>(max(1, GetFrameMax() - GetFrameMin()));
    const bool isSelectedTrack = (_view->_selectedNotifyTrackIndex == trackIndex);

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mousePos = io.MousePos;
    const bool rowHovered = rc.Contains(mousePos);
    bool clickedAnyItem = false;

    if (isSelectedTrack)
    {
        drawList->AddRectFilled(
            ImVec2(rc.Min.x, rc.Min.y + 1.f),
            ImVec2(rc.Max.x, rc.Max.y - 1.f),
            TrackColors::NotifyTrack,
            3.f);
    }

    for (auto& entry : animationData->notifies)
    {
        if (entry.trackIndex != trackIndex)
            continue;

        const int32 frame = TimeSec_ToFrame(entry.timeSec, fps);
        const float x = rc.Min.x + static_cast<float>(frame - GetFrameMin()) * pixelsPerFrame;
        const float markerWidth = 88.f;
        const float markerHeight = (rc.Max.y - rc.Min.y) - 6.f;
        const float clampedX = std::clamp(x, rc.Min.x, max(rc.Min.x, rc.Max.x - markerWidth));

        ImRect itemRect(
            ImVec2(clampedX, rc.Min.y + 3.f),
            ImVec2(clampedX + markerWidth, rc.Min.y + 3.f + markerHeight));

        const bool selected = (_view->_selectedNotifyId == entry.id);
        const bool hovered = itemRect.Contains(mousePos);

        const ImU32 fillColor = hovered
            ? TrackColors::NotifyHover
            : TrackColors::NotifyFill;

        const ImU32 borderColor = selected
            ? TrackColors::SelectedBorder
            : TrackColors::NotifyBorder;

        drawList->AddRectFilled(itemRect.Min, itemRect.Max, fillColor, 3.f);
        drawList->AddRect(itemRect.Min, itemRect.Max, borderColor, 3.f, 0, selected ? 2.f : 1.0f);

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
            _view->_selectedNotifyId = entry.id;
            _view->_selectedStateId = -1;
            _view->_selectedNotifyTrackIndex = trackIndex;
            _view->_sequencerSelectedRow = kSequencerNotifyHeaderRow + 1 + trackIndex;
            _draggingNotifyId = entry.id;
            _notifyDragOffsetX = mousePos.x - itemRect.Min.x;
        }

        if (_draggingNotifyId == entry.id && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const float localX = std::clamp(mousePos.x - _notifyDragOffsetX, rc.Min.x, rc.Max.x);
            const float relative = localX - rc.Min.x;
            const int32 draggedFrame = GetFrameMin() + static_cast<int32>(std::round(relative / max(pixelsPerFrame, 0.0001f)));

            entry.timeSec = std::clamp(
                Frame_ToTimeSec(draggedFrame, fps),
                0.f,
                animationLengthSec);

            _view->_sequencerCurrentFrame = draggedFrame;
            _view->_isDirty = true;

            if (_view->_sourceMode == Animation_View::SourceMode::LiveModel && _view->_liveModelCom != nullptr)
            {
                _view->_liveModelCom->Set_AnimationPaused(true);
                _view->_liveModelCom->Set_CurrentAnimationTimeSec(entry.timeSec);
                _view->_liveModelCom->Refresh_CurrentAnimationPose();
            }
        }
    }

    if (_draggingNotifyId != -1 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        AnimNotify_Parser::Sort_And_Clamp(*animationData, animationLengthSec);
        _draggingNotifyId = -1;
        _notifyDragOffsetX = 0.f;
    }

    if (!clickedAnyItem && rowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        _view->_selectedNotifyId = -1;
        _view->_selectedStateId = -1;
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
    const float pixelsPerFrame = width / static_cast<float>(max(1, GetFrameMax() - GetFrameMin()));
    const bool isSelectedTrack = (_view->_selectedNotifyStateTrackIndex == trackIndex);

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mousePos = io.MousePos;
    const bool rowHovered = rc.Contains(mousePos);
    bool clickedAnyItem = false;

    if (isSelectedTrack)
    {
        drawList->AddRectFilled(
            ImVec2(rc.Min.x, rc.Min.y + 1.f),
            ImVec2(rc.Max.x, rc.Max.y - 1.f),
            TrackColors::StateTrack,
            3.f);
    }

    for (auto& entry : animationData->notifyStates)
    {
        if (entry.trackIndex != trackIndex)
            continue;

        const int32 startFrame = TimeSec_ToFrame(entry.startSec, fps);
        const int32 endFrame = max(startFrame + 1, TimeSec_ToFrame(entry.startSec + entry.durationSec, fps));
        const string typeName = entry.notifyState ? entry.notifyState->Get_TypeName() : string{};
        const float minItemWidth = max(88.f, ImGui::CalcTextSize(typeName.c_str()).x + 30.f);

        const float startX = rc.Min.x + static_cast<float>(startFrame - GetFrameMin()) * pixelsPerFrame;
        const float endX = rc.Min.x + static_cast<float>(endFrame - GetFrameMin()) * pixelsPerFrame;

        ImRect itemRect(
            ImVec2(startX, rc.Min.y + 4.f),
            ImVec2(max(endX, startX + minItemWidth), rc.Max.y - 4.f));

        const bool selected = (_view->_selectedStateId == entry.id);
        const bool hovered = itemRect.Contains(mousePos);

        const float handleWidth = 10.f;
        ImRect leftHandle(
            ImVec2(itemRect.Min.x, itemRect.Min.y),
            ImVec2(itemRect.Min.x + handleWidth, itemRect.Max.y));

        ImRect rightHandle(
            ImVec2(itemRect.Max.x - handleWidth, itemRect.Min.y),
            ImVec2(itemRect.Max.x, itemRect.Max.y));

        const bool hoverLeft = leftHandle.Contains(mousePos);
        const bool hoverRight = rightHandle.Contains(mousePos);

        const ImU32 fillColor = hovered
            ? TrackColors::StateHover
            : TrackColors::StateFill;

        const ImU32 borderColor = selected
            ? TrackColors::SelectedBorder
            : TrackColors::StateBorder;

        drawList->AddRectFilled(itemRect.Min, itemRect.Max, fillColor, 3.f);
        drawList->AddRect(itemRect.Min, itemRect.Max, borderColor, 3.f, 0, selected ? 2.5f : 1.0f);
        drawList->AddRectFilled(leftHandle.Min, leftHandle.Max, hoverLeft ? TrackColors::StateHandleHover : TrackColors::StateHandle);
        drawList->AddRectFilled(rightHandle.Min, rightHandle.Max, hoverRight ? TrackColors::StateHandleHover : TrackColors::StateHandle);

        if (!typeName.empty())
        {
            const ImVec2 textSize = ImGui::CalcTextSize(typeName.c_str());
            const float textY = itemRect.Min.y + ((itemRect.Max.y - itemRect.Min.y) - textSize.y) * 0.5f;
            drawList->AddText(
                ImVec2(itemRect.Min.x + 12.f, textY),
                TrackColors::Text,
                typeName.c_str());
        }

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            clickedAnyItem = true;
            _view->_selectedStateId = entry.id;
            _view->_selectedNotifyId = -1;
            _view->_selectedNotifyStateTrackIndex = trackIndex;
            _view->_sequencerSelectedRow = Get_StateHeaderRow(animationData) + 1 + trackIndex;

            _draggingStateId = entry.id;
            _stateDragMode = hoverLeft ? EStateDragMode::ResizeStart :
                (hoverRight ? EStateDragMode::ResizeEnd : EStateDragMode::Move);
            _stateDragOffsetX = mousePos.x - itemRect.Min.x;
        }

        if (_draggingStateId == entry.id && ImGui::IsMouseDown(ImGuiMouseButton_Left) && rowHovered)
        {
            const int32 currentStartFrame = TimeSec_ToFrame(entry.startSec, fps);
            const int32 currentDurationFrame = max(1, TimeSec_ToFrame(entry.durationSec, fps));
            const int32 currentEndFrame = currentStartFrame + currentDurationFrame;

            if (_stateDragMode == EStateDragMode::ResizeStart)
            {
                const float clampedX = std::clamp(mousePos.x, rc.Min.x, rc.Max.x);
                const float relative = clampedX - rc.Min.x;
                const int32 draggedFrame = GetFrameMin() + static_cast<int32>(std::round(relative / max(pixelsPerFrame, 0.0001f)));
                const int32 newStartFrame = std::clamp(draggedFrame, GetFrameMin(), currentEndFrame - 1);
                entry.startSec = Frame_ToTimeSec(newStartFrame, fps);
                entry.durationSec = Frame_ToTimeSec(currentEndFrame - newStartFrame, fps);
            }
            else if (_stateDragMode == EStateDragMode::ResizeEnd)
            {
                const float clampedX = std::clamp(mousePos.x, rc.Min.x, rc.Max.x);
                const float relative = clampedX - rc.Min.x;
                const int32 draggedFrame = GetFrameMin() + static_cast<int32>(std::round(relative / max(pixelsPerFrame, 0.0001f)));
                const int32 newEndFrame = std::clamp(draggedFrame, currentStartFrame + 1, GetFrameMax());
                entry.durationSec = Frame_ToTimeSec(newEndFrame - currentStartFrame, fps);
            }
            else
            {
                const float localX = std::clamp(mousePos.x - _stateDragOffsetX, rc.Min.x, rc.Max.x);
                const float relative = localX - rc.Min.x;
                const int32 draggedFrame = GetFrameMin() + static_cast<int32>(std::round(relative / max(pixelsPerFrame, 0.0001f)));
                const int32 newStartFrame = std::clamp(draggedFrame, GetFrameMin(), max(GetFrameMin(), GetFrameMax() - currentDurationFrame));
                entry.startSec = Frame_ToTimeSec(newStartFrame, fps);
            }

            _view->_sequencerCurrentFrame = TimeSec_ToFrame(entry.startSec, fps);
            _view->_isDirty = true;

            if (_view->_sourceMode == Animation_View::SourceMode::LiveModel && _view->_liveModelCom != nullptr)
            {
                _view->_liveModelCom->Set_AnimationPaused(true);
                _view->_liveModelCom->Set_CurrentAnimationTimeSec(entry.startSec);
                _view->_liveModelCom->Refresh_CurrentAnimationPose();
            }
        }
    }

    if (!clickedAnyItem && rowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        _view->_selectedStateId = -1;
        _view->_selectedNotifyId = -1;
        _view->_selectedNotifyStateTrackIndex = trackIndex;
        _view->_sequencerSelectedRow = Get_StateHeaderRow(animationData) + 1 + trackIndex;
    }

    if (_view->_sequencerMarkingState &&
        _view->_sequencerContextIsStateTrack &&
        _view->_sequencerContextTrackIndex == trackIndex)
    {
        const int32 startFrame = min(_view->_sequencerMarkStartFrame, _view->_sequencerMarkEndFrame);
        const int32 endFrame = max(_view->_sequencerMarkStartFrame, _view->_sequencerMarkEndFrame);
        const float startX = rc.Min.x + static_cast<float>(startFrame - GetFrameMin()) * pixelsPerFrame;
        const float endX = rc.Min.x + static_cast<float>(endFrame - GetFrameMin()) * pixelsPerFrame;

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

    if (_draggingStateId != -1 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        AnimNotify_Parser::Sort_And_Clamp(*animationData, animationLengthSec);
        _draggingStateId = -1;
        _stateDragMode = EStateDragMode::None;
        _stateDragOffsetX = 0.f;
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
    const float pixelsPerFrame = width / static_cast<float>(max(1, GetFrameMax() - GetFrameMin()));

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
        const int32 startFrame = GetFrameMin() + static_cast<int32>(std::round(relative / max(pixelsPerFrame, 0.0001f)));
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
        _view->_sequencerMarkEndFrame = GetFrameMin() + static_cast<int32>(std::round(relative / max(pixelsPerFrame, 0.0001f)));
        return;
    }

    if (_view->_sequencerMarkingState &&
        _view->_sequencerContextIsStateTrack &&
        _view->_sequencerContextTrackIndex == trackIndex &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        const float clampedX = std::clamp(io.MousePos.x, rc.Min.x, rc.Max.x);
        const float relative = clampedX - rc.Min.x;
        _view->_sequencerMarkEndFrame = GetFrameMin() + static_cast<int32>(std::round(relative / max(pixelsPerFrame, 0.0001f)));
        _view->_sequencerMarkingState = false;
    }
}

NS_END
