#include "CinematicSequencerAdapter.h"
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
struct KeyDragState
{
    int trackIndex = -1;
    int keyIndex = -1;
};

KeyDragState g_keyDragState{};

void DrawKeyMarkers(
    CINEMATIC_SEQUENCE* sequence,
    int* selectedTrackIndex,
    int* selectedKeyIndex,
    int* currentFrame,
    bool* consumedKeyClick,
    bool* requestOpenKeyPopup,
    bool* requestOpenInsertPopup,
    bool* requestRemoveKeyPopup,
    int* contextTrackIndex,
    int* contextFrame,
    int index,
    ImDrawList* draw_list,
    const ImRect& rc,
    const ImRect& clippingRect,
    bool allowSelection)
{
    if (sequence == nullptr || draw_list == nullptr)
        return;

    if (index < 0 || index >= static_cast<int>(sequence->cinematicTracks.size()))
        return;

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        g_keyDragState = {};

    CINEMATIC_TRACK& track = sequence->cinematicTracks[index];
    const int frameMin = sequence->startFrame;
    const int frameMax = sequence->endFrame;
    const int frameRange = std::max(frameMax - frameMin, 1);
    bool isHoveringAnyKey = false;

    draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);

    for (int keyIndex = 0; keyIndex < static_cast<int>(track.keyframes.size()); ++keyIndex)
    {
        const auto& keyframe = track.keyframes[keyIndex];
        const float normalized = static_cast<float>(keyframe.frame - frameMin) / static_cast<float>(frameRange);
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
                *currentFrame = keyframe.frame;

            if (consumedKeyClick != nullptr)
                *consumedKeyClick = true;

            g_keyDragState.trackIndex = index;
            g_keyDragState.keyIndex = keyIndex;
        }

        if(isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            if(requestRemoveKeyPopup != nullptr)
                *requestRemoveKeyPopup = true;

        }

        if (isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (requestOpenKeyPopup != nullptr)
                *requestOpenKeyPopup = true;
        }

        const bool isDraggingThisKey =
            allowSelection &&
            g_keyDragState.trackIndex == index &&
            g_keyDragState.keyIndex == keyIndex &&
            ImGui::IsMouseDown(ImGuiMouseButton_Left);

        if (isDraggingThisKey)
        {
            const float mouseX = std::max(rc.Min.x, std::min(ImGui::GetIO().MousePos.x, rc.Max.x));
            const float normalizedMouse = (mouseX - rc.Min.x) / std::max(rc.Max.x - rc.Min.x, 1.f);
            const int rawFrame = frameMin + static_cast<int>(std::lround(normalizedMouse * static_cast<float>(frameRange)));

            int minFrame = frameMin;
            int maxFrame = frameMax;

            if (keyIndex > 0)
                minFrame = std::max(minFrame, track.keyframes[keyIndex - 1].frame);

            if (keyIndex + 1 < static_cast<int>(track.keyframes.size()))
                maxFrame = std::min(maxFrame, track.keyframes[keyIndex + 1].frame);

            track.keyframes[keyIndex].frame = std::clamp(rawFrame, minFrame, maxFrame);

            if (currentFrame != nullptr)
                *currentFrame = track.keyframes[keyIndex].frame;
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

        draw_list->AddLine(markerTop, markerBottom, outlineColor, 6.f);
        draw_list->AddLine(markerTop, markerBottom, lineColor, 3.5f);
        draw_list->AddConvexPolyFilled(diamond, 4, fillColor);
        draw_list->AddPolyline(diamond, 4, outlineColor, ImDrawFlags_Closed, isSelected ? 3.5f : 2.5f);
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

    draw_list->PopClipRect();
}
}

CinematicSequencerAdapter::CinematicSequencerAdapter(
    CINEMATIC_SEQUENCE* sequence,
    int* selectedTrackIndex,
    int* selectedKeyIndex,
    int* currentFrame,
    bool* consumedKeyClick,
    bool* requestOpenKeyPopup,
    bool* requestOpenInsertPopup,
    bool* requestRemoveKeyPopup,
    int* contextTrackIndex,
    int* contextFrame)
    : _sequence(sequence)
    , _selectedTrackIndex(selectedTrackIndex)
    , _selectedKeyIndex(selectedKeyIndex)
    , _currentFrame(currentFrame)
    , _consumedKeyClick(consumedKeyClick)
    , _requestOpenKeyPopup(requestOpenKeyPopup)
    , _requestOpenInsertPopup(requestOpenInsertPopup)
    , _requestRemoveKeyPopup(requestRemoveKeyPopup)
    , _contextTrackIndex(contextTrackIndex)
    , _contextFrame(contextFrame)
{
}

int CinematicSequencerAdapter::GetFrameMin() const
{
    CHECK_NULL(_sequence, 0);

    return _sequence->startFrame;
}

int CinematicSequencerAdapter::GetFrameMax() const
{
    CHECK_NULL(_sequence, 0);

    return _sequence->endFrame;
}

int CinematicSequencerAdapter::GetItemCount() const
{
    CHECK_NULL(_sequence, 0);

    return static_cast<int>(_sequence->cinematicTracks.size());
}

const char* CinematicSequencerAdapter::GetItemLabel(int index) const
{
    CHECK_NULL(_sequence, "");

    if (index < 0 || index >= static_cast<int>(_sequence->cinematicTracks.size()))
        return "";

    return _sequence->cinematicTracks[index].name.c_str();
}

void CinematicSequencerAdapter::Get(int index, int** start, int** end, int* type, unsigned int* color)
{
    CHECK_NULL(_sequence);

    if (index < 0 || index >= static_cast<int>(_sequence->cinematicTracks.size()))
        return;

    if (start != nullptr)
        *start = &_sequence->startFrame;

    if (end != nullptr)
        *end = &_sequence->endFrame;

    if (type != nullptr)
        *type = 0;

    if (color != nullptr)
        *color = 0xFFB78A2B;
}

size_t CinematicSequencerAdapter::GetCustomHeight(int index)
{
    UNREFERENCED_PARAMETER(index);

    return 0;
}

void CinematicSequencerAdapter::CustomDraw(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& legendRect, const ImRect& clippingRect, const ImRect& legendClippingRect)
{
    UNREFERENCED_PARAMETER(legendRect);
    UNREFERENCED_PARAMETER(legendClippingRect);

    DrawKeyMarkers(
        _sequence,
        _selectedTrackIndex,
        _selectedKeyIndex,
        _currentFrame,
        _consumedKeyClick,
        _requestOpenKeyPopup,
        _requestOpenInsertPopup,
        _requestRemoveKeyPopup,
        _contextTrackIndex,
        _contextFrame,
        index,
        draw_list,
        rc,
        clippingRect,
        true);
}

void CinematicSequencerAdapter::CustomDrawCompact(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& clippingRect)
{
    DrawKeyMarkers(
        _sequence,
        _selectedTrackIndex,
        _selectedKeyIndex,
        _currentFrame,
        _consumedKeyClick,
        _requestOpenKeyPopup,
        _requestOpenInsertPopup,
        _requestRemoveKeyPopup,
        _contextTrackIndex,
        _contextFrame,
        index,
        draw_list,
        rc,
        clippingRect,
        true);
}
