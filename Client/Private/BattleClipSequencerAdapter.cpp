#include "BattleClipSequencerAdapter.h"
#include <algorithm>
#include <cmath>

#ifdef new
#pragma push_macro("new")
#undef new
#define CLIENT_RESTORE_NEW_MACRO
#endif

#include <imgui.h>
#include <imgui_internal.h>

#ifdef CLIENT_RESTORE_NEW_MACRO
#pragma pop_macro("new")
#undef CLIENT_RESTORE_NEW_MACRO
#endif

namespace
{
constexpr int kBattleClipFps = 30;

struct BattleClipKeyDragState
{
    int trackIndex = -1;
    int keyIndex = -1;
};

BattleClipKeyDragState g_battleClipKeyDragState {};

int TimeToFrame(float timeSeconds)
{
    return static_cast<int>(std::lround(timeSeconds * static_cast<float>(kBattleClipFps)));
}

float FrameToTime(int frame)
{
    return static_cast<float>(frame) / static_cast<float>(kBattleClipFps);
}

int KeyTimeToFrame(float keyTime, int frameMax, bool normalizedTime)
{
    if (normalizedTime)
        return static_cast<int>(std::lround(std::clamp(keyTime, 0.f, 1.f) * static_cast<float>(std::max(frameMax, 1))));

    return TimeToFrame(keyTime);
}

float FrameToKeyTime(int frame, int frameMax, bool normalizedTime)
{
    if (normalizedTime)
        return std::clamp(static_cast<float>(frame) / static_cast<float>(std::max(frameMax, 1)), 0.f, 1.f);

    return FrameToTime(frame);
}

BattleCameraTrack* GetTrackByIndex(BattleCameraClip* clip, int index)
{
    if (clip == nullptr)
        return nullptr;

    switch (index)
    {
    case 0:
        return &clip->distanceOffset;
    case 1:
        return &clip->fovOffset;
    case 2:
        return &clip->shakeAmplitude;
    case 3:
        return &clip->sideOffset;
    case 4:
        return &clip->pivotOffset;
    case 5:
        return &clip->yawOffset;
    case 6:
        return &clip->orbitYaw;
    case 7:
        return &clip->orbitRadius;
    case 8:
        return &clip->orbitHeight;
    case 9:
        return &clip->orbitLookHeight;
    case 10:
        return &clip->shakeFrequencyX;
    case 11:
        return &clip->shakeFrequencyY;
    case 12:
        return &clip->shakeDecayRate;
    case 13:
        return &clip->shakeVerticalRatio;
    case 14:
        return &clip->shakeStrengthScale;
    default:
        return nullptr;
    }
}

const char* GetTrackLabel(int index)
{
    switch (index)
    {
    case 0:
        return "Distance Offset";
    case 1:
        return "FOV Offset";
    case 2:
        return "Shake Amplitude";
    case 3:
        return "Side Offset";
    case 4:
        return "Pivot Offset";
    case 5:
        return "Yaw Offset";
    case 6:
        return "Orbit Yaw";
    case 7:
        return "Orbit Radius";
    case 8:
        return "Orbit Height";
    case 9:
        return "Orbit Look Height";
    case 10:
        return "Shake Frequency X";
    case 11:
        return "Shake Frequency Y";
    case 12:
        return "Shake Decay Rate";
    case 13:
        return "Shake Vertical Ratio";
    case 14:
        return "Shake Strength Scale";
    default:
        return "";
    }
}

void DrawBattleClipKeyMarkers(
    BattleCameraClip* clip,
    int* selectedTrackIndex,
    int* selectedKeyIndex,
    int* currentFrame,
    bool* consumedKeyClick,
    bool* requestOpenInsertPopup,
    bool* requestRemoveKeyPopup,
    int* contextTrackIndex,
    int* contextFrame,
    int* displayFrameMax,
    bool normalizedTime,
    int index,
    ImDrawList* drawList,
    const ImRect& rc,
    const ImRect& clippingRect,
    bool allowSelection)
{
    if (clip == nullptr || drawList == nullptr)
        return;

    BattleCameraTrack* track = GetTrackByIndex(clip, index);
    if (track == nullptr)
        return;

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        g_battleClipKeyDragState = {};

    const int frameMin = 0;
    const int normalizedFrameMax = (displayFrameMax != nullptr) ? std::max(*displayFrameMax, 1) : 1;
    const int clipFrameMax = normalizedTime ? normalizedFrameMax : std::max(TimeToFrame(clip->duration), 1);
    const int frameMax = (displayFrameMax != nullptr)
        ? std::max(*displayFrameMax, clipFrameMax)
        : clipFrameMax;
    const int frameRange = std::max(frameMax - frameMin, 1);
    const float authoringDurationSec = normalizedTime ? 1.f : FrameToTime(frameMax);
    bool isHoveringAnyKey = false;

    drawList->PushClipRect(clippingRect.Min, clippingRect.Max, true);

    for (int keyIndex = 0; keyIndex < static_cast<int>(track->keyframes.size()); ++keyIndex)
    {
        BattleCameraKeyframe& keyframe = track->keyframes[keyIndex];
        const int keyFrame = std::clamp(KeyTimeToFrame(keyframe.time, frameMax, normalizedTime), frameMin, frameMax);
        const float normalized = static_cast<float>(keyFrame - frameMin) / static_cast<float>(frameRange);
        const float markerX = std::max(rc.Min.x, std::min(rc.Min.x + normalized * (rc.Max.x - rc.Min.x), rc.Max.x));
        const float centerY = (rc.Min.y + rc.Max.y) * 0.5f;
        const bool isSelected =
            (selectedTrackIndex != nullptr && selectedKeyIndex != nullptr &&
             *selectedTrackIndex == index && *selectedKeyIndex == keyIndex);

        const float handleHalfWidth = isSelected ? 10.f : 8.f;
        const float handleHalfHeight = isSelected ? 11.f : 9.f;
        const ImVec2 handleMin(markerX - handleHalfWidth, centerY - handleHalfHeight);
        const ImVec2 handleMax(markerX + handleHalfWidth, centerY + handleHalfHeight);
        const ImRect handleRect(handleMin, handleMax);
        const bool isHovered = allowSelection && ImGui::IsMouseHoveringRect(handleRect.Min, handleRect.Max, false);
        if (isHovered)
            isHoveringAnyKey = true;

        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (selectedTrackIndex != nullptr)
                *selectedTrackIndex = index;

            if (selectedKeyIndex != nullptr)
                *selectedKeyIndex = keyIndex;

            if (currentFrame != nullptr)
                *currentFrame = keyFrame;

            if (consumedKeyClick != nullptr)
                *consumedKeyClick = true;

            g_battleClipKeyDragState.trackIndex = index;
            g_battleClipKeyDragState.keyIndex = keyIndex;
        }

        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            if (selectedTrackIndex != nullptr)
                *selectedTrackIndex = index;

            if (selectedKeyIndex != nullptr)
                *selectedKeyIndex = keyIndex;

            if (requestRemoveKeyPopup != nullptr)
                *requestRemoveKeyPopup = true;
        }

        const bool isDraggingThisKey =
            allowSelection &&
            g_battleClipKeyDragState.trackIndex == index &&
            g_battleClipKeyDragState.keyIndex == keyIndex &&
            ImGui::IsMouseDown(ImGuiMouseButton_Left);

        if (isDraggingThisKey)
        {
            const float mouseX = std::max(rc.Min.x, std::min(ImGui::GetIO().MousePos.x, rc.Max.x));
            const float normalizedMouse = (mouseX - rc.Min.x) / std::max(rc.Max.x - rc.Min.x, 1.f);
            const int rawFrame = frameMin + static_cast<int>(std::lround(normalizedMouse * static_cast<float>(frameRange)));

            int minFrame = frameMin;
            int maxFrame = frameMax;

            if (keyIndex > 0)
                minFrame = std::max(minFrame, KeyTimeToFrame(track->keyframes[keyIndex - 1].time, frameMax, normalizedTime));

            if (keyIndex + 1 < static_cast<int>(track->keyframes.size()))
                maxFrame = std::min(maxFrame, KeyTimeToFrame(track->keyframes[keyIndex + 1].time, frameMax, normalizedTime));

            const int clampedFrame = std::clamp(rawFrame, minFrame, maxFrame);
            keyframe.time = std::clamp(FrameToKeyTime(clampedFrame, frameMax, normalizedTime), 0.f, authoringDurationSec);
            if (!normalizedTime && keyframe.time > clip->duration)
                clip->duration = keyframe.time;

            if (currentFrame != nullptr)
                *currentFrame = clampedFrame;
        }

        const ImU32 lineColor = isSelected ? 0xFFFFFFB3 : 0xFFFFD166;
        const ImU32 fillColor = isSelected ? 0xFFFFCC33 : (isHovered ? 0xFFFFE699 : 0xFFE1B657);
        const ImU32 outlineColor = isSelected ? 0xFF4B3300 : 0xFF3A2E12;
        const ImVec2 markerTop(markerX, rc.Min.y + 3.f);
        const ImVec2 markerBottom(markerX, rc.Max.y - 3.f);
        const ImVec2 diamond[4] =
        {
            ImVec2(markerX, centerY - handleHalfHeight),
            ImVec2(markerX + handleHalfWidth, centerY),
            ImVec2(markerX, centerY + handleHalfHeight),
            ImVec2(markerX - handleHalfWidth, centerY)
        };

        drawList->AddLine(markerTop, markerBottom, outlineColor, 6.f);
        drawList->AddLine(markerTop, markerBottom, lineColor, 3.5f);
        drawList->AddConvexPolyFilled(diamond, 4, fillColor);
        drawList->AddPolyline(diamond, 4, outlineColor, ImDrawFlags_Closed, isSelected ? 3.5f : 2.5f);
    }

    const bool isRowHovered = allowSelection && ImGui::IsMouseHoveringRect(rc.Min, rc.Max, false);
    if (isRowHovered && !isHoveringAnyKey && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        const float mouseX = std::max(rc.Min.x, std::min(ImGui::GetIO().MousePos.x, rc.Max.x));
        const float normalizedMouse = (mouseX - rc.Min.x) / std::max(rc.Max.x - rc.Min.x, 1.f);
        const int clickedFrame = frameMin + static_cast<int>(std::lround(normalizedMouse * static_cast<float>(frameRange)));

        if (selectedTrackIndex != nullptr)
            *selectedTrackIndex = index;

        if (currentFrame != nullptr)
            *currentFrame = std::clamp(clickedFrame, frameMin, frameMax);

        if (contextTrackIndex != nullptr)
            *contextTrackIndex = index;

        if (contextFrame != nullptr)
            *contextFrame = std::clamp(clickedFrame, frameMin, frameMax);

        if (requestOpenInsertPopup != nullptr)
            *requestOpenInsertPopup = true;
    }

    drawList->PopClipRect();
}
}

BattleClipSequencerAdapter::BattleClipSequencerAdapter(
    BattleCameraClip* clip,
    int* selectedTrackIndex,
    int* selectedKeyIndex,
    int* currentFrame,
    bool* consumedKeyClick,
    bool* requestOpenInsertPopup,
    bool* requestRemoveKeyPopup,
    int* contextTrackIndex,
    int* contextFrame,
    int* displayFrameMax,
    bool normalizedTime,
    bool* previewDirty,
    bool* distanceEnabled,
    bool* fovEnabled,
    bool* shakeEnabled,
    bool* sideEnabled,
    bool* pivotEnabled,
    bool* yawEnabled,
    bool* orbitYawEnabled,
    bool* orbitRadiusEnabled,
    bool* orbitHeightEnabled,
    bool* orbitLookHeightEnabled,
    bool* shakeFrequencyXEnabled,
    bool* shakeFrequencyYEnabled,
    bool* shakeDecayRateEnabled,
    bool* shakeVerticalRatioEnabled,
    bool* shakeStrengthScaleEnabled)
    : _clip(clip)
    , _selectedTrackIndex(selectedTrackIndex)
    , _selectedKeyIndex(selectedKeyIndex)
    , _currentFrame(currentFrame)
    , _consumedKeyClick(consumedKeyClick)
    , _requestOpenInsertPopup(requestOpenInsertPopup)
    , _requestRemoveKeyPopup(requestRemoveKeyPopup)
    , _contextTrackIndex(contextTrackIndex)
    , _contextFrame(contextFrame)
    , _displayFrameMax(displayFrameMax)
    , _normalizedTime(normalizedTime)
    , _previewDirty(previewDirty)
{
    _trackEnabledFlags[0] = distanceEnabled;
    _trackEnabledFlags[1] = fovEnabled;
    _trackEnabledFlags[2] = shakeEnabled;
    _trackEnabledFlags[3] = sideEnabled;
    _trackEnabledFlags[4] = pivotEnabled;
    _trackEnabledFlags[5] = yawEnabled;
    _trackEnabledFlags[6] = orbitYawEnabled;
    _trackEnabledFlags[7] = orbitRadiusEnabled;
    _trackEnabledFlags[8] = orbitHeightEnabled;
    _trackEnabledFlags[9] = orbitLookHeightEnabled;
    _trackEnabledFlags[10] = shakeFrequencyXEnabled;
    _trackEnabledFlags[11] = shakeFrequencyYEnabled;
    _trackEnabledFlags[12] = shakeDecayRateEnabled;
    _trackEnabledFlags[13] = shakeVerticalRatioEnabled;
    _trackEnabledFlags[14] = shakeStrengthScaleEnabled;
}

int BattleClipSequencerAdapter::GetFrameMin() const
{
    return 0;
}

int BattleClipSequencerAdapter::GetFrameMax() const
{
    if (GetClip() == nullptr)
        return 1;

    const int clipFrameMax = std::max(TimeToFrame(GetClip()->duration), 1);
    if (_displayFrameMax == nullptr)
        return clipFrameMax;

    if (_normalizedTime)
        return std::max(*_displayFrameMax, 1);

    return std::max(clipFrameMax, std::max(*_displayFrameMax, 1));
}

int BattleClipSequencerAdapter::GetItemCount() const
{
    return kBattleClipTrackCount;
}

const char* BattleClipSequencerAdapter::GetItemLabel(int index) const
{
    return GetTrackLabel(index);
}

void BattleClipSequencerAdapter::Get(int index, int** start, int** end, int* type, unsigned int* color)
{
    UNREFERENCED_PARAMETER(index);

    _frameMin = GetFrameMin();
    _frameMax = GetFrameMax();

    if (start != nullptr)
        *start = &_frameMin;

    if (end != nullptr)
        *end = &_frameMax;

    if (type != nullptr)
        *type = 0;

    if (color != nullptr)
        *color = 0xFFB78A2B;
}

size_t BattleClipSequencerAdapter::GetCustomHeight(int index)
{
    UNREFERENCED_PARAMETER(index);
    return 0;
}

bool* BattleClipSequencerAdapter::GetTrackEnabledFlag(int index) const
{
    if (index < 0 || index >= static_cast<int>(std::size(_trackEnabledFlags)))
        return nullptr;

    return _trackEnabledFlags[index];
}

void BattleClipSequencerAdapter::CustomDraw(int index, ImDrawList* drawList, const ImRect& rc, const ImRect& legendRect, const ImRect& clippingRect, const ImRect& legendClippingRect)
{
    if (bool* enabled = GetTrackEnabledFlag(index))
    {
        const float checkboxSize = 16.f;
        const ImVec2 checkboxMin(
            legendRect.Max.x - checkboxSize - 8.f,
            legendRect.Min.y + std::max((legendRect.GetHeight() - checkboxSize) * 0.5f, 0.f));
        const ImVec2 checkboxMax(checkboxMin.x + checkboxSize, checkboxMin.y + checkboxSize);
        const ImRect checkboxRect(checkboxMin, checkboxMax);
        const bool hovered = ImGui::IsMouseHoveringRect(checkboxRect.Min, checkboxRect.Max, false);

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            *enabled = !(*enabled);
            if (_previewDirty != nullptr)
                *_previewDirty = true;
        }

        drawList->PushClipRect(legendClippingRect.Min, legendClippingRect.Max, true);
        drawList->AddRectFilled(
            checkboxRect.Min,
            checkboxRect.Max,
            hovered ? IM_COL32(74, 76, 84, 255) : IM_COL32(52, 54, 60, 255),
            3.f);
        drawList->AddRect(checkboxRect.Min, checkboxRect.Max, IM_COL32(130, 134, 146, 255), 3.f);

        if (*enabled)
        {
            drawList->AddLine(
                ImVec2(checkboxRect.Min.x + 3.f, checkboxRect.Min.y + checkboxSize * 0.55f),
                ImVec2(checkboxRect.Min.x + checkboxSize * 0.42f, checkboxRect.Max.y - 3.f),
                IM_COL32(242, 194, 76, 255),
                2.f);
            drawList->AddLine(
                ImVec2(checkboxRect.Min.x + checkboxSize * 0.42f, checkboxRect.Max.y - 3.f),
                ImVec2(checkboxRect.Max.x - 3.f, checkboxRect.Min.y + 3.f),
                IM_COL32(242, 194, 76, 255),
                2.f);
        }

        drawList->PopClipRect();
    }

    DrawBattleClipKeyMarkers(
        _clip,
        _selectedTrackIndex,
        _selectedKeyIndex,
        _currentFrame,
        _consumedKeyClick,
        _requestOpenInsertPopup,
        _requestRemoveKeyPopup,
        _contextTrackIndex,
        _contextFrame,
        _displayFrameMax,
        _normalizedTime,
        index,
        drawList,
        rc,
        clippingRect,
        true);
}

void BattleClipSequencerAdapter::CustomDrawCompact(int index, ImDrawList* drawList, const ImRect& rc, const ImRect& clippingRect)
{
    DrawBattleClipKeyMarkers(
        _clip,
        _selectedTrackIndex,
        _selectedKeyIndex,
        _currentFrame,
        _consumedKeyClick,
        _requestOpenInsertPopup,
        _requestRemoveKeyPopup,
        _contextTrackIndex,
        _contextFrame,
        _displayFrameMax,
        _normalizedTime,
        index,
        drawList,
        rc,
        clippingRect,
        true);
}
