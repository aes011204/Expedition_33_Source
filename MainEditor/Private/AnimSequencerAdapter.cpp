#include "AnimSequencerAdapter.h"
#include "Animation_View.h"
#include "ModelCom.h"

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

    const int32 fps = _view->Get_CurrentAnimationFps();
    const float lengthSec = _view->Get_CurrentAnimationLengthSec();
    return max(1, TimeSec_ToFrame(lengthSec, fps));
}

int AnimSequencerAdapter::GetItemCount() const
{
    return 2;
}

const char* AnimSequencerAdapter::GetItemLabel(int index) const
{
    return (index == 0) ? "Notifies" : "Notify States";
}

void AnimSequencerAdapter::Get(int index, int** start, int** end, int* type, unsigned* color)
{
    _rowStart[index] = GetFrameMin();
    _rowEnd[index] = GetFrameMax();

    if (start) *start = &_rowStart[index];
    if (end) *end = &_rowEnd[index];
    if (type) *type = index;
    if (color) *color = (index == 0) ? IM_COL32(60, 180, 90, 255) : IM_COL32(70, 140, 255, 255);
}

void AnimSequencerAdapter::CustomDraw(int index, ImDrawList* drawList, const ImRect& rc, const ImRect& legendRect,
    const ImRect& clippingRect, const ImRect& legendClippingRect)
{
    SequenceInterface::CustomDraw(index, drawList, rc, legendRect, clippingRect, legendClippingRect);

    if (_view == nullptr)
        return;

    if (index == 0)
        Draw_NotifyRow(drawList, rc);
    else
        Draw_StateRow(drawList, rc);
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

void AnimSequencerAdapter::Draw_NotifyRow(ImDrawList* drawList, const ImRect& rc)
{
    if (_view == nullptr)
        return;

    AnimNotifyData* animationData = _view->Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    const int32 fps = max(1, _view->Get_CurrentAnimationFps());
    const float animationLengthSec = _view->Get_CurrentAnimationLengthSec();
    const float width = rc.Max.x - rc.Min.x;
    const float height = rc.Max.y - rc.Min.y;
    const float pixelsPerFrame = width / static_cast<float>(max(1, GetFrameMax() - GetFrameMin()));

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mousePos = io.MousePos;
    const bool isHovered = rc.Contains(mousePos);

    for (auto& entry : animationData->notifies)
    {
        const int32 frame = TimeSec_ToFrame(entry.timeSec, fps);
        const float centerX = rc.Min.x + static_cast<float>(frame - GetFrameMin()) * pixelsPerFrame;
        const float barHalfWidth = 4.f;

        ImVec2 itemMin(centerX - barHalfWidth, rc.Min.y + 4.f);
        ImVec2 itemMax(centerX + barHalfWidth, rc.Max.y - 4.f);
        ImRect itemRect(itemMin, itemMax);

        const bool selected = (_view->_selectedNotifyId == entry.id);
        const bool hovered = itemRect.Contains(mousePos);

        const ImU32 fillColor = hovered
            ? IM_COL32(80, 220, 120, 255)
            : IM_COL32(60, 180, 90, 255);

        const ImU32 borderColor = selected
            ? IM_COL32(0, 255, 255, 255)
            : IM_COL32(20, 90, 40, 255);

        drawList->AddRectFilled(itemRect.Min, itemRect.Max, fillColor, 2.f);
        drawList->AddRect(itemRect.Min, itemRect.Max, borderColor, 2.f, 0, selected ? 2.5f : 1.0f);

        if (!entry.name.empty())
        {
            drawList->AddText(
                ImVec2(itemRect.Max.x + 4.f, rc.Min.y + 3.f),
                IM_COL32(230, 230, 230, 255),
                entry.name.c_str());
        }

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            _view->_selectedNotifyId = entry.id;
            _view->_selectedStateId = -1;
            _view->_sequencerSelectedRow = 0;
        }

        if (selected && isHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            const float localX = std::clamp(mousePos.x, rc.Min.x, rc.Max.x);
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
}

void AnimSequencerAdapter::Draw_StateRow(ImDrawList* drawList, const ImRect& rc)
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

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mousePos = io.MousePos;
    const bool rowHovered = rc.Contains(mousePos);

    static int32 s_DraggingStateId = -1;
    static bool s_ResizeStart = false;
    static bool s_ResizeEnd = false;

    for (auto& entry : animationData->notifyStates)
    {
        const int32 startFrame = TimeSec_ToFrame(entry.startSec, fps);
        const int32 endFrame = TimeSec_ToFrame(entry.startSec + entry.durationSec, fps);

        const float startX = rc.Min.x + static_cast<float>(startFrame - GetFrameMin()) * pixelsPerFrame;
        const float endX = rc.Min.x + static_cast<float>(endFrame - GetFrameMin()) * pixelsPerFrame;

        ImRect itemRect(
            ImVec2(startX, rc.Min.y + 6.f),
            ImVec2(max(endX, startX + 8.f), rc.Max.y - 6.f));

        const bool selected = (_view->_selectedStateId == entry.id);
        const bool hovered = itemRect.Contains(mousePos);

        const float handleWidth = 6.f;
        ImRect leftHandle(
            ImVec2(itemRect.Min.x, itemRect.Min.y),
            ImVec2(itemRect.Min.x + handleWidth, itemRect.Max.y));

        ImRect rightHandle(
            ImVec2(itemRect.Max.x - handleWidth, itemRect.Min.y),
            ImVec2(itemRect.Max.x, itemRect.Max.y));

        const bool hoverLeft = leftHandle.Contains(mousePos);
        const bool hoverRight = rightHandle.Contains(mousePos);

        const ImU32 fillColor = hovered
            ? IM_COL32(90, 160, 255, 255)
            : IM_COL32(70, 140, 255, 255);

        const ImU32 borderColor = selected
            ? IM_COL32(0, 255, 255, 255)
            : IM_COL32(30, 70, 130, 255);

        drawList->AddRectFilled(itemRect.Min, itemRect.Max, fillColor, 3.f);
        drawList->AddRect(itemRect.Min, itemRect.Max, borderColor, 3.f, 0, selected ? 2.5f : 1.0f);
        drawList->AddRectFilled(leftHandle.Min, leftHandle.Max, IM_COL32(35, 80, 150, 255));
        drawList->AddRectFilled(rightHandle.Min, rightHandle.Max, IM_COL32(35, 80, 150, 255));

        if (!entry.name.empty())
        {
            drawList->AddText(
                ImVec2(itemRect.Min.x + 8.f, itemRect.Min.y + 2.f),
                IM_COL32(240, 240, 240, 255),
                entry.name.c_str());
        }

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            _view->_selectedStateId = entry.id;
            _view->_selectedNotifyId = -1;
            _view->_sequencerSelectedRow = 1;

            s_DraggingStateId = entry.id;
            s_ResizeStart = hoverLeft;
            s_ResizeEnd = hoverRight;
        }

        if (selected && s_DraggingStateId == entry.id && ImGui::IsMouseDown(ImGuiMouseButton_Left) && rowHovered)
        {
            const float clampedX = std::clamp(mousePos.x, rc.Min.x, rc.Max.x);
            const float relative = clampedX - rc.Min.x;
            const int32 draggedFrame = GetFrameMin() + static_cast<int32>(std::round(relative / max(pixelsPerFrame, 0.0001f)));
            const float draggedTimeSec = std::clamp(Frame_ToTimeSec(draggedFrame, fps), 0.f, animationLengthSec);

            if (s_ResizeStart)
            {
                const float stateEndSec = entry.startSec + entry.durationSec;
                entry.startSec = std::min(draggedTimeSec, stateEndSec);
                entry.durationSec = max(0.f, stateEndSec - entry.startSec);
            }
            else if (s_ResizeEnd)
            {
                entry.durationSec = max(0.f, draggedTimeSec - entry.startSec);
            }
            else
            {
                const float oldDuration = entry.durationSec;
                entry.startSec = draggedTimeSec;
                entry.durationSec = oldDuration;

                if (entry.startSec + entry.durationSec > animationLengthSec)
                    entry.startSec = max(0.f, animationLengthSec - entry.durationSec);
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

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        s_DraggingStateId = -1;
        s_ResizeStart = false;
        s_ResizeEnd = false;
    }
}
