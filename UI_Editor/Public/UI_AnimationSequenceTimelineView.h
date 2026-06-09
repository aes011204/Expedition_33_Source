#pragma once

#include "UI_AnimationTypes.h"

namespace UIEditor
{
    struct UI_AnimationSequenceTimelineClickResult
    {
        bool clicked = false;
        int32 frame = 0;
        int32 clipIndex = -1;
    };

    class UI_AnimationSequenceTimelineView final
    {
    public:
        UI_AnimationSequenceTimelineView() = default;
        ~UI_AnimationSequenceTimelineView() = default;

    public:
        UI_AnimationSequenceTimelineClickResult Render(
            const UI_AnimationSequenceAsset& sequence,
            int32 currentFrame,
            int32 selectedClipIndex,
            const ImVec2& size);

    private:
        int32 Find_ClipIndexAtFrame(const UI_AnimationSequenceAsset& sequence, int32 frame) const;
    };
}
