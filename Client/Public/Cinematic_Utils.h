#pragma once
#include "Cinematic_Types.h"

inline void Find_SurroundingKeyFrames(CINEMATIC_TRACK* cameraTrack, int currentFrame, CINEMATIC_KEYFRAME*& outLeft, CINEMATIC_KEYFRAME*& outRight)
{
    outLeft = nullptr;
    outRight = nullptr;

    if (cameraTrack == nullptr || cameraTrack->keyframes.empty())
        return;

    for (auto& keyframe : cameraTrack->keyframes)
    {
        if (keyframe.frame <= currentFrame)
            outLeft = &keyframe;

        if (keyframe.frame >= currentFrame)
        {
            outRight = &keyframe;
            break;
        }
    }
}
