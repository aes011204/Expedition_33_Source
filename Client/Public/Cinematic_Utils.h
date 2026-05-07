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
//Tool 쪽에서는 어떻게 쓰냐..
//Evaluate_CinematicPose를 호출해서 이걸로 player에 적용을 해서 그 시점으로 카메라가 볼 수 있도록 처리를 ㅐㅎ줘야하는건데
//그럼 player에 ApplyPose 함수가 있으니까 이걸 호출하는 식으로? 


inline CAMERAPOSE Evaluate_CinematicPose(CINEMATIC_TRACK* cameraTrack, int currentFrame, CINEMATIC_KEYFRAME*& outLeft, CINEMATIC_KEYFRAME*& outRight)
{
    Find_SurroundingKeyFrames(cameraTrack, currentFrame, outLeft, outRight);

    CAMERAPOSE camPose {};

    if(outLeft == nullptr && outRight == nullptr)
        return camPose;

    if(outLeft == nullptr)
    {
        camPose.position = outRight->camPose.position;
        camPose.rotation = outRight->camPose.rotation;
        camPose.fovY = outRight->camPose.fovY;
        camPose.nearPlane = outRight->camPose.nearPlane;
        camPose.farPlane = outRight->camPose.farPlane;
        return camPose;
    }

    if(outRight == nullptr)
    {
        camPose.position = outLeft->camPose.position;
        camPose.rotation = outLeft->camPose.rotation;
        camPose.fovY = outLeft->camPose.fovY;
        camPose.nearPlane = outLeft->camPose.nearPlane;
        camPose.farPlane = outLeft->camPose.farPlane;
        return camPose;
    }

    if(outRight->frame == outLeft->frame)
    {
        camPose.position = outLeft->camPose.position;
        camPose.rotation = outLeft->camPose.rotation;
        camPose.fovY = outLeft->camPose.fovY;
        camPose.nearPlane = outLeft->camPose.nearPlane;
        camPose.farPlane = outLeft->camPose.farPlane;
        return camPose;
    }

    float t = static_cast<float>(currentFrame - outLeft->frame) / static_cast<float>(outRight->frame - outLeft->frame);

    camPose.position = Vec3::Lerp(outLeft->camPose.position,outRight->camPose.position,t);
    camPose.rotation = Vec3::Lerp(outLeft->camPose.rotation, outRight->camPose.rotation, t);
    camPose.fovY    = lerp(outLeft->camPose.fovY, outRight->camPose.fovY, t);
    camPose.nearPlane = lerp(outLeft->camPose.nearPlane, outRight->camPose.nearPlane, t);
    camPose.farPlane = lerp(outLeft->camPose.farPlane, outRight->camPose.farPlane, t);

    return camPose;
}
