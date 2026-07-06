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
bool g_blocksFrameScrubThisFrame = false;

constexpr int kModifierTrackCount = CinematicSequencerAdapter::kModifierTrackCount;
constexpr int kEventTrackCount = CinematicSequencerAdapter::kEventTrackCount;

const char* GetModifierTrackLabel(int modifierTrackIndex)
{
    switch (modifierTrackIndex)
    {
    case 0: return "Shake Amplitude";
    case 1: return "Shake Frequency X";
    case 2: return "Shake Frequency Y";
    case 3: return "Shake Decay Rate X";
    case 4: return "Shake Decay Rate Y";
    case 5: return "Shake Vertical Ratio";
    case 6: return "Shake Strength Scale";
    default: return "";
    }
}

bool IsModifierTrackIndex(const CINEMATIC_SEQUENCE* sequence, int index)
{
    if (sequence == nullptr)
        return false;

    const int startIndex = static_cast<int>(sequence->cinematicTracks.size()) +
        static_cast<int>(sequence->actorRelativeCameraTracks.size());
    return index >= startIndex && index < startIndex + kModifierTrackCount;
}

int GetModifierTrackIndex(const CINEMATIC_SEQUENCE* sequence, int index)
{
    if (!IsModifierTrackIndex(sequence, index))
        return -1;

    return index - static_cast<int>(sequence->cinematicTracks.size()) - static_cast<int>(sequence->actorRelativeCameraTracks.size());
}

int GetEventTrackIndex(const CINEMATIC_SEQUENCE* sequence)
{
    if (sequence == nullptr)
        return -1;

    return static_cast<int>(sequence->cinematicTracks.size()) +
        static_cast<int>(sequence->actorRelativeCameraTracks.size()) +
        kModifierTrackCount;
}

int GetActorRelativeTrackStartIndex(const CINEMATIC_SEQUENCE* sequence)
{
    if (sequence == nullptr)
        return -1;

    return static_cast<int>(sequence->cinematicTracks.size());
}

bool IsActorRelativeTrackIndex(const CINEMATIC_SEQUENCE* sequence, int index)
{
    if (sequence == nullptr)
        return false;

    const int startIndex = GetActorRelativeTrackStartIndex(sequence);
    return index >= startIndex &&
        index < startIndex + static_cast<int>(sequence->actorRelativeCameraTracks.size());
}

int GetActorRelativeTrackOffset(const CINEMATIC_SEQUENCE* sequence, int index)
{
    if (!IsActorRelativeTrackIndex(sequence, index))
        return -1;

    return index - GetActorRelativeTrackStartIndex(sequence);
}

bool IsEventTrackIndex(const CINEMATIC_SEQUENCE* sequence, int index)
{
    return sequence != nullptr && index == GetEventTrackIndex(sequence);
}

int GetActorTransformTrackStartIndex(const CINEMATIC_SEQUENCE* sequence)
{
    if (sequence == nullptr)
        return -1;

    return GetEventTrackIndex(sequence) + kEventTrackCount;
}

bool IsActorTransformTrackIndex(const CINEMATIC_SEQUENCE* sequence, int index)
{
    if (sequence == nullptr)
        return false;

    const int startIndex = GetActorTransformTrackStartIndex(sequence);
    return index >= startIndex &&
        index < startIndex + static_cast<int>(sequence->actorTransformTracks.size());
}

int GetActorTransformTrackOffset(const CINEMATIC_SEQUENCE* sequence, int index)
{
    if (!IsActorTransformTrackIndex(sequence, index))
        return -1;

    return index - GetActorTransformTrackStartIndex(sequence);
}

int GetPartRotationTrackStartIndex(const CINEMATIC_SEQUENCE* sequence)
{
    if (sequence == nullptr)
        return -1;

    return GetActorTransformTrackStartIndex(sequence) + static_cast<int>(sequence->actorTransformTracks.size());
}

bool IsPartRotationTrackIndex(const CINEMATIC_SEQUENCE* sequence, int index)
{
    if (sequence == nullptr)
        return false;

    const int startIndex = GetPartRotationTrackStartIndex(sequence);
    return index >= startIndex &&
        index < startIndex + static_cast<int>(sequence->partRotationTracks.size());
}

int GetPartRotationTrackOffset(const CINEMATIC_SEQUENCE* sequence, int index)
{
    if (!IsPartRotationTrackIndex(sequence, index))
        return -1;

    return index - GetPartRotationTrackStartIndex(sequence);
}

bool* GetPreviewToggleForTrack(
    const CINEMATIC_SEQUENCE* sequence,
    int index,
    bool* cameraTrackEnabled,
    bool* eventTrackEnabled,
    bool* actorTransformTrackEnabled)
{
    if (sequence == nullptr)
        return nullptr;

    if (index >= 0 && index < static_cast<int>(sequence->cinematicTracks.size()))
        return cameraTrackEnabled;

    if (IsActorRelativeTrackIndex(sequence, index))
        return cameraTrackEnabled;

    if (IsEventTrackIndex(sequence, index))
        return eventTrackEnabled;

    if (IsActorTransformTrackIndex(sequence, index))
        return actorTransformTrackEnabled;

    if (IsPartRotationTrackIndex(sequence, index))
        return actorTransformTrackEnabled;

    return nullptr;
}

CameraTrack* GetModifierTrack(CINEMATIC_SEQUENCE* sequence, int index)
{
    if (sequence == nullptr)
        return nullptr;

    switch (GetModifierTrackIndex(sequence, index))
    {
    case 0: return &sequence->modifierClip.shakeAmplitude;
    case 1: return &sequence->modifierClip.shakeFrequencyX;
    case 2: return &sequence->modifierClip.shakeFrequencyY;
    case 3: return &sequence->modifierClip.shakeDecayRateX;
    case 4: return &sequence->modifierClip.shakeDecayRate;
    case 5: return &sequence->modifierClip.shakeVerticalRatio;
    case 6: return &sequence->modifierClip.shakeStrengthScale;
    default: return nullptr;
    }
}

float FrameToModifierTime(const CINEMATIC_SEQUENCE* sequence, int frame)
{
    if (sequence == nullptr)
        return 0.f;

    const int fps = std::max(sequence->fps, 1);
    return static_cast<float>(std::max(frame - sequence->startFrame, 0)) / static_cast<float>(fps);
}

int ModifierTimeToFrame(const CINEMATIC_SEQUENCE* sequence, float time)
{
    if (sequence == nullptr)
        return 0;

    const int fps = std::max(sequence->fps, 1);
    return sequence->startFrame + static_cast<int>(std::lround(std::max(time, 0.f) * static_cast<float>(fps)));
}

void DrawCinematicKeyMarkers(
    CINEMATIC_TRACK& track,
    int* selectedTrackIndex,
    int* selectedKeyIndex,
    int* currentFrame,
    bool* consumedKeyClick,
    bool* requestOpenKeyPopup,
    bool* requestRemoveKeyPopup,
    int index,
    const ImRect& rc,
    int frameMin,
    int frameMax,
    bool* outIsHoveringAnyKey)
{
    const int frameRange = std::max(frameMax - frameMin, 1);

    for (int keyIndex = 0; keyIndex < static_cast<int>(track.keyframes.size()); ++keyIndex)
    {
        auto& keyframe = track.keyframes[keyIndex];
        const float normalized = static_cast<float>(keyframe.frame - frameMin) / static_cast<float>(frameRange);
        const float markerX = std::max(rc.Min.x, std::min(rc.Min.x + normalized * (rc.Max.x - rc.Min.x), rc.Max.x));
        const float centerY = (rc.Min.y + rc.Max.y) * 0.5f;
        const bool isSelected =
            selectedTrackIndex != nullptr && selectedKeyIndex != nullptr &&
            *selectedTrackIndex == index && *selectedKeyIndex == keyIndex;

        const float handleHalfWidth = isSelected ? 10.f : 8.f;
        const float handleHalfHeight = isSelected ? 11.f : 9.f;
        const ImRect handleRect(
            ImVec2(markerX - handleHalfWidth, centerY - handleHalfHeight),
            ImVec2(markerX + handleHalfWidth, centerY + handleHalfHeight));
        const bool isHovered = ImGui::IsMouseHoveringRect(handleRect.Min, handleRect.Max, false);
        if (isHovered && outIsHoveringAnyKey != nullptr)
            *outIsHoveringAnyKey = true;
        if (isHovered)
            g_blocksFrameScrubThisFrame = true;
        if (isHovered)
            g_blocksFrameScrubThisFrame = true;

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

        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            if (selectedTrackIndex != nullptr)
                *selectedTrackIndex = index;
            if (selectedKeyIndex != nullptr)
                *selectedKeyIndex = keyIndex;
            if (requestRemoveKeyPopup != nullptr)
                *requestRemoveKeyPopup = true;
        }

        if (isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (requestOpenKeyPopup != nullptr)
                *requestOpenKeyPopup = true;
        }

        const bool isDraggingThisKey =
            g_keyDragState.trackIndex == index &&
            g_keyDragState.keyIndex == keyIndex &&
            ImGui::IsMouseDown(ImGuiMouseButton_Left);

        if (isDraggingThisKey)
        {
            g_blocksFrameScrubThisFrame = true;
            const float mouseX = std::max(rc.Min.x, std::min(ImGui::GetIO().MousePos.x, rc.Max.x));
            const float normalizedMouse = (mouseX - rc.Min.x) / std::max(rc.Max.x - rc.Min.x, 1.f);
            const int rawFrame = frameMin + static_cast<int>(std::lround(normalizedMouse * static_cast<float>(frameRange)));

            track.keyframes[keyIndex].frame = std::clamp(rawFrame, frameMin, frameMax);
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

        ImGui::GetWindowDrawList()->AddLine(markerTop, markerBottom, outlineColor, 6.f);
        ImGui::GetWindowDrawList()->AddLine(markerTop, markerBottom, lineColor, 3.5f);
        ImGui::GetWindowDrawList()->AddConvexPolyFilled(diamond, 4, fillColor);
        ImGui::GetWindowDrawList()->AddPolyline(diamond, 4, outlineColor, ImDrawFlags_Closed, isSelected ? 3.5f : 2.5f);
    }
}

void DrawModifierKeyMarkers(
    CINEMATIC_SEQUENCE* sequence,
    CameraTrack& track,
    int* selectedTrackIndex,
    int* selectedKeyIndex,
    int* currentFrame,
    bool* consumedKeyClick,
    bool* requestOpenKeyPopup,
    bool* requestRemoveKeyPopup,
    int index,
    const ImRect& rc,
    int frameMin,
    int frameMax,
    bool* outIsHoveringAnyKey)
{
    const int frameRange = std::max(frameMax - frameMin, 1);

    for (int keyIndex = 0; keyIndex < static_cast<int>(track.keyframes.size()); ++keyIndex)
    {
        auto& keyframe = track.keyframes[keyIndex];
        const int keyFrame = std::clamp(ModifierTimeToFrame(sequence, keyframe.time), frameMin, frameMax);
        const float normalized = static_cast<float>(keyFrame - frameMin) / static_cast<float>(frameRange);
        const float markerX = std::max(rc.Min.x, std::min(rc.Min.x + normalized * (rc.Max.x - rc.Min.x), rc.Max.x));
        const float centerY = (rc.Min.y + rc.Max.y) * 0.5f;
        const bool isSelected =
            selectedTrackIndex != nullptr && selectedKeyIndex != nullptr &&
            *selectedTrackIndex == index && *selectedKeyIndex == keyIndex;

        const float handleHalfWidth = isSelected ? 10.f : 8.f;
        const float handleHalfHeight = isSelected ? 11.f : 9.f;
        const ImRect handleRect(
            ImVec2(markerX - handleHalfWidth, centerY - handleHalfHeight),
            ImVec2(markerX + handleHalfWidth, centerY + handleHalfHeight));
        const bool isHovered = ImGui::IsMouseHoveringRect(handleRect.Min, handleRect.Max, false);
        if (isHovered && outIsHoveringAnyKey != nullptr)
            *outIsHoveringAnyKey = true;

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

            g_keyDragState.trackIndex = index;
            g_keyDragState.keyIndex = keyIndex;
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

        if (isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (requestOpenKeyPopup != nullptr)
                *requestOpenKeyPopup = true;
        }

        const bool isDraggingThisKey =
            g_keyDragState.trackIndex == index &&
            g_keyDragState.keyIndex == keyIndex &&
            ImGui::IsMouseDown(ImGuiMouseButton_Left);

        if (isDraggingThisKey)
        {
            g_blocksFrameScrubThisFrame = true;
            const float mouseX = std::max(rc.Min.x, std::min(ImGui::GetIO().MousePos.x, rc.Max.x));
            const float normalizedMouse = (mouseX - rc.Min.x) / std::max(rc.Max.x - rc.Min.x, 1.f);
            const int rawFrame = frameMin + static_cast<int>(std::lround(normalizedMouse * static_cast<float>(frameRange)));

            const int clampedFrame = std::clamp(rawFrame, frameMin, frameMax);
            keyframe.time = FrameToModifierTime(sequence, clampedFrame);
            if (currentFrame != nullptr)
                *currentFrame = clampedFrame;
        }

        const ImU32 lineColor = isSelected ? 0xFFFFFFB3 : 0xFF7FDBFF;
        const ImU32 fillColor = isSelected ? 0xFF3FA7D6 : (isHovered ? 0xFF8FD4F5 : 0xFF4FA9CE);
        const ImU32 outlineColor = isSelected ? 0xFF0E3347 : 0xFF12384A;
        const ImVec2 markerTop(markerX, rc.Min.y + 3.f);
        const ImVec2 markerBottom(markerX, rc.Max.y - 3.f);
        const ImVec2 diamond[4] =
        {
            ImVec2(markerX, centerY - handleHalfHeight),
            ImVec2(markerX + handleHalfWidth, centerY),
            ImVec2(markerX, centerY + handleHalfHeight),
            ImVec2(markerX - handleHalfWidth, centerY)
        };

        ImGui::GetWindowDrawList()->AddLine(markerTop, markerBottom, outlineColor, 6.f);
        ImGui::GetWindowDrawList()->AddLine(markerTop, markerBottom, lineColor, 3.5f);
        ImGui::GetWindowDrawList()->AddConvexPolyFilled(diamond, 4, fillColor);
        ImGui::GetWindowDrawList()->AddPolyline(diamond, 4, outlineColor, ImDrawFlags_Closed, isSelected ? 3.5f : 2.5f);
    }
}

void DrawKeyMarkers(
    CINEMATIC_SEQUENCE* sequence,
    int* selectedTrackIndex,
    int* selectedKeyIndex,
    int* selectedEventIndex,
    int* selectedActorRelativeTrackIndex,
    int* selectedActorRelativeKeyIndex,
    int* selectedActorTransformTrackIndex,
    int* selectedActorTransformKeyIndex,
    int* selectedPartRotationTrackIndex,
    int* selectedPartRotationKeyIndex,
    int* currentFrame,
    bool* consumedKeyClick,
    bool* requestOpenKeyPopup,
    bool* requestOpenInsertPopup,
    bool* requestRemoveKeyPopup,
    int* contextTrackIndex,
    int* contextFrame,
    int visibleFrameMin,
    int visibleFrameMax,
    int index,
    ImDrawList* draw_list,
    const ImRect& rc,
    const ImRect& clippingRect,
    bool allowSelection)
{
    if (sequence == nullptr || draw_list == nullptr)
        return;

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        g_keyDragState = {};

    const int frameMin = std::clamp(visibleFrameMin, sequence->startFrame, sequence->endFrame);
    const int frameMax = std::clamp(visibleFrameMax, frameMin + 1, std::max(sequence->startFrame + 1, sequence->endFrame));
    bool isHoveringAnyKey = false;

    draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);

    if (index >= 0 && index < static_cast<int>(sequence->cinematicTracks.size()))
    {
        DrawCinematicKeyMarkers(
            sequence->cinematicTracks[index],
            selectedTrackIndex,
            selectedKeyIndex,
            currentFrame,
            consumedKeyClick,
            requestOpenKeyPopup,
            requestRemoveKeyPopup,
            index,
            rc,
            frameMin,
            frameMax,
            &isHoveringAnyKey);
    }
    else if (IsActorRelativeTrackIndex(sequence, index))
    {
        const int frameRange = std::max(frameMax - frameMin, 1);
        const int actorRelativeTrackIndex = GetActorRelativeTrackOffset(sequence, index);
        auto& track = sequence->actorRelativeCameraTracks[actorRelativeTrackIndex];

        for (int keyIndex = 0; keyIndex < static_cast<int>(track.keyframes.size()); ++keyIndex)
        {
            auto& keyframe = track.keyframes[keyIndex];
            const float normalized = static_cast<float>(keyframe.frame - frameMin) / static_cast<float>(frameRange);
            const float markerX = std::max(rc.Min.x, std::min(rc.Min.x + normalized * (rc.Max.x - rc.Min.x), rc.Max.x));
            const float centerY = (rc.Min.y + rc.Max.y) * 0.5f;
            const bool isSelected =
                selectedActorRelativeTrackIndex != nullptr &&
                selectedActorRelativeKeyIndex != nullptr &&
                *selectedActorRelativeTrackIndex == actorRelativeTrackIndex &&
                *selectedActorRelativeKeyIndex == keyIndex;

            const float handleHalfWidth = isSelected ? 9.f : 7.f;
            const float handleHalfHeight = isSelected ? 10.f : 8.f;
            const ImRect handleRect(
                ImVec2(markerX - handleHalfWidth, centerY - handleHalfHeight),
                ImVec2(markerX + handleHalfWidth, centerY + handleHalfHeight));
            const bool isHovered = ImGui::IsMouseHoveringRect(handleRect.Min, handleRect.Max, false);
            if (isHovered)
                isHoveringAnyKey = true;
            if (isHovered)
                g_blocksFrameScrubThisFrame = true;
            if (isHovered)
                g_blocksFrameScrubThisFrame = true;
            if (isHovered)
                g_blocksFrameScrubThisFrame = true;
            if (isHovered)
                g_blocksFrameScrubThisFrame = true;

            if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                if (selectedActorRelativeTrackIndex != nullptr)
                    *selectedActorRelativeTrackIndex = actorRelativeTrackIndex;
                if (selectedActorRelativeKeyIndex != nullptr)
                    *selectedActorRelativeKeyIndex = keyIndex;
                if (selectedTrackIndex != nullptr)
                    *selectedTrackIndex = index;
                if (currentFrame != nullptr)
                    *currentFrame = keyframe.frame;
                if (consumedKeyClick != nullptr)
                    *consumedKeyClick = true;

                g_keyDragState.trackIndex = index;
                g_keyDragState.keyIndex = keyIndex;
            }

            if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                if (selectedTrackIndex != nullptr)
                    *selectedTrackIndex = index;
                if (selectedActorRelativeTrackIndex != nullptr)
                    *selectedActorRelativeTrackIndex = actorRelativeTrackIndex;
                if (selectedActorRelativeKeyIndex != nullptr)
                    *selectedActorRelativeKeyIndex = keyIndex;
                if (requestRemoveKeyPopup != nullptr)
                    *requestRemoveKeyPopup = true;
            }

            if (isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if (requestOpenKeyPopup != nullptr)
                    *requestOpenKeyPopup = true;
            }

            const bool isDraggingThisKey =
                g_keyDragState.trackIndex == index &&
                g_keyDragState.keyIndex == keyIndex &&
                ImGui::IsMouseDown(ImGuiMouseButton_Left);

            if (isDraggingThisKey)
            {
                g_blocksFrameScrubThisFrame = true;
                const float mouseX = std::max(rc.Min.x, std::min(ImGui::GetIO().MousePos.x, rc.Max.x));
                const float normalizedMouse = (mouseX - rc.Min.x) / std::max(rc.Max.x - rc.Min.x, 1.f);
                const int rawFrame = frameMin + static_cast<int>(std::lround(normalizedMouse * static_cast<float>(frameRange)));

                keyframe.frame = std::clamp(rawFrame, frameMin, frameMax);
                if (currentFrame != nullptr)
                    *currentFrame = keyframe.frame;
            }

            const ImU32 lineColor = isSelected ? 0xFFEAF0FF : 0xFFAAB8FF;
            const ImU32 fillColor = isSelected ? 0xFF6F86FF : (isHovered ? 0xFFC4CEFF : 0xFF8D9BFF);
            const ImU32 outlineColor = isSelected ? 0xFF273A8C : 0xFF30438E;
            const ImVec2 markerTop(markerX, rc.Min.y + 3.f);
            const ImVec2 markerBottom(markerX, rc.Max.y - 3.f);
            const ImVec2 diamond[4] =
            {
                ImVec2(markerX, centerY - handleHalfHeight),
                ImVec2(markerX + handleHalfWidth, centerY),
                ImVec2(markerX, centerY + handleHalfHeight),
                ImVec2(markerX - handleHalfWidth, centerY)
            };

            ImGui::GetWindowDrawList()->AddLine(markerTop, markerBottom, outlineColor, 6.f);
            ImGui::GetWindowDrawList()->AddLine(markerTop, markerBottom, lineColor, 3.f);
            ImGui::GetWindowDrawList()->AddConvexPolyFilled(diamond, 4, fillColor);
            ImGui::GetWindowDrawList()->AddPolyline(diamond, 4, outlineColor, ImDrawFlags_Closed, isSelected ? 3.f : 2.f);
        }
    }
    else if (CameraTrack* modifierTrack = GetModifierTrack(sequence, index))
    {
        DrawModifierKeyMarkers(
            sequence,
            *modifierTrack,
            selectedTrackIndex,
            selectedKeyIndex,
            currentFrame,
            consumedKeyClick,
            requestOpenKeyPopup,
            requestRemoveKeyPopup,
            index,
            rc,
            frameMin,
            frameMax,
            &isHoveringAnyKey);
    }
    else if (IsEventTrackIndex(sequence, index))
    {
        const int frameRange = std::max(frameMax - frameMin, 1);

        for (int eventIndex = 0; eventIndex < static_cast<int>(sequence->eventTrack.size()); ++eventIndex)
        {
            auto& event = sequence->eventTrack[eventIndex];
            const float normalized = static_cast<float>(event.frame - frameMin) / static_cast<float>(frameRange);
            const float markerX = std::max(rc.Min.x, std::min(rc.Min.x + normalized * (rc.Max.x - rc.Min.x), rc.Max.x));
            const float centerY = (rc.Min.y + rc.Max.y) * 0.5f;
            const bool isSelected = selectedEventIndex != nullptr && *selectedEventIndex == eventIndex;

            const float handleHalfWidth = isSelected ? 9.f : 7.f;
            const float handleHalfHeight = isSelected ? 10.f : 8.f;
            const ImRect handleRect(
                ImVec2(markerX - handleHalfWidth, centerY - handleHalfHeight),
                ImVec2(markerX + handleHalfWidth, centerY + handleHalfHeight));
            const bool isHovered = ImGui::IsMouseHoveringRect(handleRect.Min, handleRect.Max, false);
            if (isHovered)
                isHoveringAnyKey = true;

            if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                if (selectedEventIndex != nullptr)
                    *selectedEventIndex = eventIndex;
                if (currentFrame != nullptr)
                    *currentFrame = event.frame;
                if (consumedKeyClick != nullptr)
                    *consumedKeyClick = true;

                g_keyDragState.trackIndex = index;
                g_keyDragState.keyIndex = eventIndex;
            }

            const bool isDraggingThisEvent =
                g_keyDragState.trackIndex == index &&
                g_keyDragState.keyIndex == eventIndex &&
                ImGui::IsMouseDown(ImGuiMouseButton_Left);

            if (isDraggingThisEvent)
            {
                g_blocksFrameScrubThisFrame = true;
                const float mouseX = std::max(rc.Min.x, std::min(ImGui::GetIO().MousePos.x, rc.Max.x));
                const float normalizedMouse = (mouseX - rc.Min.x) / std::max(rc.Max.x - rc.Min.x, 1.f);
                const int rawFrame = frameMin + static_cast<int>(std::lround(normalizedMouse * static_cast<float>(frameRange)));

                event.frame = std::clamp(rawFrame, frameMin, frameMax);
                if (currentFrame != nullptr)
                    *currentFrame = event.frame;
            }

            const ImU32 lineColor = isSelected ? 0xFFFFF2D0 : 0xFFFFB347;
            const ImU32 fillColor = isSelected ? 0xFFFF8C42 : (isHovered ? 0xFFFFD29A : 0xFFE58A2B);
            const ImU32 outlineColor = isSelected ? 0xFF5B2D00 : 0xFF4A2400;
            const ImVec2 markerTop(markerX, rc.Min.y + 3.f);
            const ImVec2 markerBottom(markerX, rc.Max.y - 3.f);
            const ImVec2 diamond[4] =
            {
                ImVec2(markerX, centerY - handleHalfHeight),
                ImVec2(markerX + handleHalfWidth, centerY),
                ImVec2(markerX, centerY + handleHalfHeight),
                ImVec2(markerX - handleHalfWidth, centerY)
            };

            ImGui::GetWindowDrawList()->AddLine(markerTop, markerBottom, outlineColor, 6.f);
            ImGui::GetWindowDrawList()->AddLine(markerTop, markerBottom, lineColor, 3.f);
            ImGui::GetWindowDrawList()->AddConvexPolyFilled(diamond, 4, fillColor);
            ImGui::GetWindowDrawList()->AddPolyline(diamond, 4, outlineColor, ImDrawFlags_Closed, isSelected ? 3.f : 2.f);
        }
    }
    else if (IsActorTransformTrackIndex(sequence, index))
    {
        const int frameRange = std::max(frameMax - frameMin, 1);
        const int actorTrackIndex = GetActorTransformTrackOffset(sequence, index);
        auto& track = sequence->actorTransformTracks[actorTrackIndex];

        for (int keyIndex = 0; keyIndex < static_cast<int>(track.keyframes.size()); ++keyIndex)
        {
            auto& keyframe = track.keyframes[keyIndex];
            const float normalized = static_cast<float>(keyframe.frame - frameMin) / static_cast<float>(frameRange);
            const float markerX = std::max(rc.Min.x, std::min(rc.Min.x + normalized * (rc.Max.x - rc.Min.x), rc.Max.x));
            const float centerY = (rc.Min.y + rc.Max.y) * 0.5f;
            const bool isSelected =
                selectedActorTransformTrackIndex != nullptr &&
                selectedActorTransformKeyIndex != nullptr &&
                *selectedActorTransformTrackIndex == actorTrackIndex &&
                *selectedActorTransformKeyIndex == keyIndex;

            const float handleHalfWidth = isSelected ? 9.f : 7.f;
            const float handleHalfHeight = isSelected ? 10.f : 8.f;
            const ImRect handleRect(
                ImVec2(markerX - handleHalfWidth, centerY - handleHalfHeight),
                ImVec2(markerX + handleHalfWidth, centerY + handleHalfHeight));
            const bool isHovered = ImGui::IsMouseHoveringRect(handleRect.Min, handleRect.Max, false);
            if (isHovered)
                isHoveringAnyKey = true;

            if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                if (selectedTrackIndex != nullptr)
                    *selectedTrackIndex = index;
                if (selectedActorTransformTrackIndex != nullptr)
                    *selectedActorTransformTrackIndex = actorTrackIndex;
                if (selectedActorTransformKeyIndex != nullptr)
                    *selectedActorTransformKeyIndex = keyIndex;
                if (currentFrame != nullptr)
                    *currentFrame = keyframe.frame;
                if (consumedKeyClick != nullptr)
                    *consumedKeyClick = true;

                g_keyDragState.trackIndex = index;
                g_keyDragState.keyIndex = keyIndex;
            }

            const bool isDraggingThisKey =
                g_keyDragState.trackIndex == index &&
                g_keyDragState.keyIndex == keyIndex &&
                ImGui::IsMouseDown(ImGuiMouseButton_Left);

            if (isDraggingThisKey)
            {
                g_blocksFrameScrubThisFrame = true;
                const float mouseX = std::max(rc.Min.x, std::min(ImGui::GetIO().MousePos.x, rc.Max.x));
                const float normalizedMouse = (mouseX - rc.Min.x) / std::max(rc.Max.x - rc.Min.x, 1.f);
                const int rawFrame = frameMin + static_cast<int>(std::lround(normalizedMouse * static_cast<float>(frameRange)));
                keyframe.frame = std::clamp(rawFrame, frameMin, frameMax);
                if (currentFrame != nullptr)
                    *currentFrame = keyframe.frame;
            }

            const ImU32 lineColor = isSelected ? 0xFFE7FFD9 : 0xFF88E06A;
            const ImU32 fillColor = isSelected ? 0xFF56C73A : (isHovered ? 0xFFB8F29E : 0xFF6FCF4A);
            const ImU32 outlineColor = isSelected ? 0xFF1D4A0F : 0xFF234F18;
            const ImVec2 markerTop(markerX, rc.Min.y + 3.f);
            const ImVec2 markerBottom(markerX, rc.Max.y - 3.f);
            const ImVec2 diamond[4] =
            {
                ImVec2(markerX, centerY - handleHalfHeight),
                ImVec2(markerX + handleHalfWidth, centerY),
                ImVec2(markerX, centerY + handleHalfHeight),
                ImVec2(markerX - handleHalfWidth, centerY)
            };

            ImGui::GetWindowDrawList()->AddLine(markerTop, markerBottom, outlineColor, 6.f);
            ImGui::GetWindowDrawList()->AddLine(markerTop, markerBottom, lineColor, 3.f);
            ImGui::GetWindowDrawList()->AddConvexPolyFilled(diamond, 4, fillColor);
            ImGui::GetWindowDrawList()->AddPolyline(diamond, 4, outlineColor, ImDrawFlags_Closed, isSelected ? 3.f : 2.f);
        }
    }
    else if (IsPartRotationTrackIndex(sequence, index))
    {
        const int frameRange = std::max(frameMax - frameMin, 1);
        const int partTrackIndex = GetPartRotationTrackOffset(sequence, index);
        auto& track = sequence->partRotationTracks[partTrackIndex];

        for (int keyIndex = 0; keyIndex < static_cast<int>(track.keyframes.size()); ++keyIndex)
        {
            auto& keyframe = track.keyframes[keyIndex];
            const float normalized = static_cast<float>(keyframe.frame - frameMin) / static_cast<float>(frameRange);
            const float markerX = std::max(rc.Min.x, std::min(rc.Min.x + normalized * (rc.Max.x - rc.Min.x), rc.Max.x));
            const float centerY = (rc.Min.y + rc.Max.y) * 0.5f;
            const bool isSelected =
                selectedPartRotationTrackIndex != nullptr &&
                selectedPartRotationKeyIndex != nullptr &&
                *selectedPartRotationTrackIndex == partTrackIndex &&
                *selectedPartRotationKeyIndex == keyIndex;

            const float handleHalfWidth = isSelected ? 9.f : 7.f;
            const float handleHalfHeight = isSelected ? 10.f : 8.f;
            const ImRect handleRect(
                ImVec2(markerX - handleHalfWidth, centerY - handleHalfHeight),
                ImVec2(markerX + handleHalfWidth, centerY + handleHalfHeight));
            const bool isHovered = ImGui::IsMouseHoveringRect(handleRect.Min, handleRect.Max, false);
            if (isHovered)
                isHoveringAnyKey = true;

            if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                if (selectedTrackIndex != nullptr)
                    *selectedTrackIndex = index;
                if (selectedPartRotationTrackIndex != nullptr)
                    *selectedPartRotationTrackIndex = partTrackIndex;
                if (selectedPartRotationKeyIndex != nullptr)
                    *selectedPartRotationKeyIndex = keyIndex;
                if (currentFrame != nullptr)
                    *currentFrame = keyframe.frame;
                if (consumedKeyClick != nullptr)
                    *consumedKeyClick = true;

                g_keyDragState.trackIndex = index;
                g_keyDragState.keyIndex = keyIndex;
            }

            if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                if (selectedTrackIndex != nullptr)
                    *selectedTrackIndex = index;
                if (selectedPartRotationTrackIndex != nullptr)
                    *selectedPartRotationTrackIndex = partTrackIndex;
                if (selectedPartRotationKeyIndex != nullptr)
                    *selectedPartRotationKeyIndex = keyIndex;
                if (requestRemoveKeyPopup != nullptr)
                    *requestRemoveKeyPopup = true;
            }

            if (isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if (requestOpenKeyPopup != nullptr)
                    *requestOpenKeyPopup = true;
            }

            const bool isDraggingThisKey =
                g_keyDragState.trackIndex == index &&
                g_keyDragState.keyIndex == keyIndex &&
                ImGui::IsMouseDown(ImGuiMouseButton_Left);

            if (isDraggingThisKey)
            {
                g_blocksFrameScrubThisFrame = true;
                const float mouseX = std::max(rc.Min.x, std::min(ImGui::GetIO().MousePos.x, rc.Max.x));
                const float normalizedMouse = (mouseX - rc.Min.x) / std::max(rc.Max.x - rc.Min.x, 1.f);
                const int rawFrame = frameMin + static_cast<int>(std::lround(normalizedMouse * static_cast<float>(frameRange)));
                keyframe.frame = std::clamp(rawFrame, frameMin, frameMax);
                if (currentFrame != nullptr)
                    *currentFrame = keyframe.frame;
            }

            const ImU32 lineColor = isSelected ? 0xFFF6E3FF : 0xFFD7A6F0;
            const ImU32 fillColor = isSelected ? 0xFFA94CD0 : (isHovered ? 0xFFE5C1F6 : 0xFFB774D8);
            const ImU32 outlineColor = isSelected ? 0xFF4B1265 : 0xFF552A68;
            const ImVec2 markerTop(markerX, rc.Min.y + 3.f);
            const ImVec2 markerBottom(markerX, rc.Max.y - 3.f);
            const ImVec2 diamond[4] =
            {
                ImVec2(markerX, centerY - handleHalfHeight),
                ImVec2(markerX + handleHalfWidth, centerY),
                ImVec2(markerX, centerY + handleHalfHeight),
                ImVec2(markerX - handleHalfWidth, centerY)
            };

            ImGui::GetWindowDrawList()->AddLine(markerTop, markerBottom, outlineColor, 6.f);
            ImGui::GetWindowDrawList()->AddLine(markerTop, markerBottom, lineColor, 3.f);
            ImGui::GetWindowDrawList()->AddConvexPolyFilled(diamond, 4, fillColor);
            ImGui::GetWindowDrawList()->AddPolyline(diamond, 4, outlineColor, ImDrawFlags_Closed, isSelected ? 3.f : 2.f);
        }
    }

    const bool isRowHovered = allowSelection && ImGui::IsMouseHoveringRect(rc.Min, rc.Max, false);
    if (isRowHovered &&
        !isHoveringAnyKey &&
        !IsEventTrackIndex(sequence, index) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        const float mouseX = std::max(rc.Min.x, std::min(ImGui::GetIO().MousePos.x, rc.Max.x));
        const float normalizedMouse = (mouseX - rc.Min.x) / std::max(rc.Max.x - rc.Min.x, 1.f);
        const int clickedFrame = frameMin + static_cast<int>(std::lround(normalizedMouse * static_cast<float>(std::max(frameMax - frameMin, 1))));

        if (IsActorTransformTrackIndex(sequence, index))
        {
            if (selectedTrackIndex != nullptr)
                *selectedTrackIndex = index;
            if (selectedActorTransformTrackIndex != nullptr)
                *selectedActorTransformTrackIndex = GetActorTransformTrackOffset(sequence, index);
            if (selectedActorTransformKeyIndex != nullptr)
                *selectedActorTransformKeyIndex = -1;
        }
        else if (IsPartRotationTrackIndex(sequence, index))
        {
            if (selectedTrackIndex != nullptr)
                *selectedTrackIndex = index;
            if (selectedPartRotationTrackIndex != nullptr)
                *selectedPartRotationTrackIndex = GetPartRotationTrackOffset(sequence, index);
            if (selectedPartRotationKeyIndex != nullptr)
                *selectedPartRotationKeyIndex = -1;
        }
        else if (IsActorRelativeTrackIndex(sequence, index))
        {
            if (selectedTrackIndex != nullptr)
                *selectedTrackIndex = index;
            if (selectedActorRelativeTrackIndex != nullptr)
                *selectedActorRelativeTrackIndex = GetActorRelativeTrackOffset(sequence, index);
            if (selectedActorRelativeKeyIndex != nullptr)
                *selectedActorRelativeKeyIndex = -1;
        }
        else if (selectedTrackIndex != nullptr)
        {
            *selectedTrackIndex = index;
        }

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
    int* selectedEventIndex,
    int* selectedActorRelativeTrackIndex,
    int* selectedActorRelativeKeyIndex,
    int* selectedActorTransformTrackIndex,
    int* selectedActorTransformKeyIndex,
    int* selectedPartRotationTrackIndex,
    int* selectedPartRotationKeyIndex,
    int* currentFrame,
    bool* consumedKeyClick,
    bool* requestOpenKeyPopup,
    bool* requestOpenInsertPopup,
    bool* requestRemoveKeyPopup,
    int* contextTrackIndex,
    int* contextFrame,
    bool* previewDirty,
    bool* cameraTrackEnabled,
    bool* eventTrackEnabled,
    bool* actorTransformTrackEnabled)
    : _sequence(sequence)
    , _selectedTrackIndex(selectedTrackIndex)
    , _selectedKeyIndex(selectedKeyIndex)
    , _selectedEventIndex(selectedEventIndex)
    , _selectedActorRelativeTrackIndex(selectedActorRelativeTrackIndex)
    , _selectedActorRelativeKeyIndex(selectedActorRelativeKeyIndex)
    , _selectedActorTransformTrackIndex(selectedActorTransformTrackIndex)
    , _selectedActorTransformKeyIndex(selectedActorTransformKeyIndex)
    , _selectedPartRotationTrackIndex(selectedPartRotationTrackIndex)
    , _selectedPartRotationKeyIndex(selectedPartRotationKeyIndex)
    , _currentFrame(currentFrame)
    , _consumedKeyClick(consumedKeyClick)
    , _requestOpenKeyPopup(requestOpenKeyPopup)
    , _requestOpenInsertPopup(requestOpenInsertPopup)
    , _requestRemoveKeyPopup(requestRemoveKeyPopup)
    , _contextTrackIndex(contextTrackIndex)
    , _contextFrame(contextFrame)
    , _previewDirty(previewDirty)
    , _cameraTrackEnabled(cameraTrackEnabled)
    , _eventTrackEnabled(eventTrackEnabled)
    , _actorTransformTrackEnabled(actorTransformTrackEnabled)
{
    if (_sequence != nullptr)
    {
        _visibleFrameMin = _sequence->startFrame;
        _visibleFrameMax = std::max(_sequence->startFrame + 1, _sequence->endFrame);
    }
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
    return static_cast<int>(_sequence->cinematicTracks.size()) +
        static_cast<int>(_sequence->actorRelativeCameraTracks.size()) +
        kModifierTrackCount +
        kEventTrackCount +
        static_cast<int>(_sequence->actorTransformTracks.size()) +
        static_cast<int>(_sequence->partRotationTracks.size());
}

const char* CinematicSequencerAdapter::GetItemLabel(int index) const
{
    CHECK_NULL(_sequence, "");

    if (index < 0 || index >= GetItemCount())
        return "";

    if (index >= 0 && index < static_cast<int>(_sequence->cinematicTracks.size()))
        return _sequence->cinematicTracks[index].name.c_str();

    if (IsActorRelativeTrackIndex(_sequence, index))
    {
        const int actorRelativeTrackIndex = GetActorRelativeTrackOffset(_sequence, index);
        return _sequence->actorRelativeCameraTracks[actorRelativeTrackIndex].name.c_str();
    }

    if (IsEventTrackIndex(_sequence, index))
        return "Event Track";

    if (IsActorTransformTrackIndex(_sequence, index))
    {
        const int actorTrackIndex = GetActorTransformTrackOffset(_sequence, index);
        return _sequence->actorTransformTracks[actorTrackIndex].name.c_str();
    }

    if (IsPartRotationTrackIndex(_sequence, index))
    {
        const int partTrackIndex = GetPartRotationTrackOffset(_sequence, index);
        return _sequence->partRotationTracks[partTrackIndex].name.c_str();
    }

    if (IsModifierTrackIndex(_sequence, index))
        return GetModifierTrackLabel(GetModifierTrackIndex(_sequence, index));

    return "";
}

void CinematicSequencerAdapter::SetVisibleFrameRange(int frameMin, int frameMax)
{
    if (_sequence == nullptr)
        return;

    g_blocksFrameScrubThisFrame = false;
    _visibleFrameMin = std::clamp(frameMin, _sequence->startFrame, _sequence->endFrame);
    _visibleFrameMax = std::clamp(frameMax, _visibleFrameMin + 1, std::max(_sequence->startFrame + 1, _sequence->endFrame));
}

bool CinematicSequencerAdapter::BlocksFrameScrub() const
{
    return g_blocksFrameScrubThisFrame;
}

void CinematicSequencerAdapter::Get(int index, int** start, int** end, int* type, unsigned int* color)
{
    CHECK_NULL(_sequence);

    if (index < 0 || index >= GetItemCount())
        return;

    if (start != nullptr)
        *start = &_sequence->startFrame;
    if (end != nullptr)
        *end = &_sequence->endFrame;
    if (type != nullptr)
        *type = 0;
    if (color != nullptr)
    {
        if (IsEventTrackIndex(_sequence, index))
            *color = 0xFFE58A2B;
        else if (IsPartRotationTrackIndex(_sequence, index))
            *color = 0xFFB774D8;
        else if (IsActorTransformTrackIndex(_sequence, index))
            *color = 0xFF6FCF4A;
        else if (IsActorRelativeTrackIndex(_sequence, index))
            *color = 0xFF8D9BFF;
        else if (IsModifierTrackIndex(_sequence, index))
            *color = 0xFF4FA9CE;
        else
            *color = 0xFFB78A2B;
    }
}

size_t CinematicSequencerAdapter::GetCustomHeight(int index)
{
    if (_sequence == nullptr || index < 0 || index >= GetItemCount())
        return 0;

    return 0;
}

void CinematicSequencerAdapter::CustomDraw(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& legendRect, const ImRect& clippingRect, const ImRect& legendClippingRect)
{
    if (_sequence == nullptr || draw_list == nullptr || index < 0 || index >= GetItemCount())
        return;

    if (bool* enabled = GetPreviewToggleForTrack(
        _sequence,
        index,
        _cameraTrackEnabled,
        _eventTrackEnabled,
        _actorTransformTrackEnabled))
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

        draw_list->PushClipRect(legendClippingRect.Min, legendClippingRect.Max, true);
        draw_list->AddRectFilled(
            checkboxRect.Min,
            checkboxRect.Max,
            hovered ? IM_COL32(74, 76, 84, 255) : IM_COL32(52, 54, 60, 255),
            3.f);
        draw_list->AddRect(checkboxRect.Min, checkboxRect.Max, IM_COL32(130, 134, 146, 255), 3.f);

        if (*enabled)
        {
            draw_list->AddLine(
                ImVec2(checkboxRect.Min.x + 3.f, checkboxRect.Min.y + checkboxSize * 0.55f),
                ImVec2(checkboxRect.Min.x + checkboxSize * 0.42f, checkboxRect.Max.y - 3.f),
                IM_COL32(242, 194, 76, 255),
                2.f);
            draw_list->AddLine(
                ImVec2(checkboxRect.Min.x + checkboxSize * 0.42f, checkboxRect.Max.y - 3.f),
                ImVec2(checkboxRect.Max.x - 3.f, checkboxRect.Min.y + 3.f),
                IM_COL32(242, 194, 76, 255),
                2.f);
        }

        draw_list->PopClipRect();
    }

    DrawKeyMarkers(
        _sequence,
        _selectedTrackIndex,
        _selectedKeyIndex,
        _selectedEventIndex,
        _selectedActorRelativeTrackIndex,
        _selectedActorRelativeKeyIndex,
        _selectedActorTransformTrackIndex,
        _selectedActorTransformKeyIndex,
        _selectedPartRotationTrackIndex,
        _selectedPartRotationKeyIndex,
        _currentFrame,
        _consumedKeyClick,
        _requestOpenKeyPopup,
        _requestOpenInsertPopup,
        _requestRemoveKeyPopup,
        _contextTrackIndex,
        _contextFrame,
        _visibleFrameMin,
        _visibleFrameMax,
        index,
        draw_list,
        rc,
        clippingRect,
        true);
}

void CinematicSequencerAdapter::CustomDrawCompact(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& clippingRect)
{
    if (_sequence == nullptr || draw_list == nullptr || index < 0 || index >= GetItemCount())
        return;

    DrawKeyMarkers(
        _sequence,
        _selectedTrackIndex,
        _selectedKeyIndex,
        _selectedEventIndex,
        _selectedActorRelativeTrackIndex,
        _selectedActorRelativeKeyIndex,
        _selectedActorTransformTrackIndex,
        _selectedActorTransformKeyIndex,
        _selectedPartRotationTrackIndex,
        _selectedPartRotationKeyIndex,
        _currentFrame,
        _consumedKeyClick,
        _requestOpenKeyPopup,
        _requestOpenInsertPopup,
        _requestRemoveKeyPopup,
        _contextTrackIndex,
        _contextFrame,
        _visibleFrameMin,
        _visibleFrameMax,
        index,
        draw_list,
        rc,
        clippingRect,
        true);
}
