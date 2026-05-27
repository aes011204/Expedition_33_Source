#pragma once

#include "UI_AnimationTypes.h"

namespace UIEditor
{
    struct UI_AnimationTimelineClickResult
    {
        bool clicked = false;
        bool hitKey = false;
        bool draggedKey = false;
        int32 frame = 0;
        int32 targetIndex = -1;
        int32 trackIndex = -1;
        int32 keyIndex = -1;
    };

    class UI_AnimationTimelineView final
    {
    public:
        UI_AnimationTimelineView() = default;
        ~UI_AnimationTimelineView() = default;

    public:
        UI_AnimationTimelineClickResult Render(
            const UI_AnimationAsset& asset,
            int32 currentFrame,
            int32 selectedTargetIndex,
            int32 selectedTrackIndex,
            int32 selectedKeyIndex,
            const ImVec2& size);

        static float Get_TrackPanelWidth();
        static float Get_KeyPanelWidth();

    private:
        bool _draggingKey = false;
        bool _dragStarted = false;
        int32 _dragTargetIndex = -1;
        int32 _dragTrackIndex = -1;
        int32 _dragKeyIndex = -1;
        ImVec2 _dragStartMousePos = ImVec2(0.f, 0.f);
    };
}
