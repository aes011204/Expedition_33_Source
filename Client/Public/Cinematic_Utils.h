#pragma once
#include "Cinematic_Types.h"
#include "BattleCameraClipUtils.h"

inline float WrapCinematicDegrees180(float degrees)
{
    float wrapped = fmodf(degrees + 180.f, 360.f);
    if (wrapped < 0.f)
        wrapped += 360.f;

    return wrapped - 180.f;
}

inline float LerpCinematicDegreesShortest(float fromDegrees, float toDegrees, float t)
{
    const float delta = WrapCinematicDegrees180(toDegrees - fromDegrees);
    return WrapCinematicDegrees180(fromDegrees + delta * t);
}

inline Vec3 LerpCinematicRotationDegreesShortest(const Vec3& fromDegrees, const Vec3& toDegrees, float t)
{
    return Vec3(
        LerpCinematicDegreesShortest(fromDegrees.x, toDegrees.x, t),
        LerpCinematicDegreesShortest(fromDegrees.y, toDegrees.y, t),
        LerpCinematicDegreesShortest(fromDegrees.z, toDegrees.z, t));
}

inline void Find_SurroundingKeyFrames(CINEMATIC_TRACK* cameraTrack, float currentFrame, CINEMATIC_KEYFRAME*& outLeft, CINEMATIC_KEYFRAME*& outRight)
{
    outLeft = nullptr;
    outRight = nullptr;

    if (cameraTrack == nullptr || cameraTrack->keyframes.empty())
        return;

    for (auto& keyframe : cameraTrack->keyframes)
    {
        if (static_cast<float>(keyframe.frame) <= currentFrame)
            outLeft = &keyframe;

        if (static_cast<float>(keyframe.frame) >= currentFrame)
        {
            outRight = &keyframe;
            break;
        }
    }
}

inline CAMERAPOSE Evaluate_LinearPose(CINEMATIC_KEYFRAME*& outLeft, CINEMATIC_KEYFRAME*& outRight, float t)
{
    CAMERAPOSE camPose {};

    camPose.position = Vec3::Lerp(outLeft->camPose.position,outRight->camPose.position,t);
    camPose.rotation = LerpCinematicRotationDegreesShortest(outLeft->camPose.rotation, outRight->camPose.rotation, t);
    camPose.fovY    = lerp(outLeft->camPose.fovY, outRight->camPose.fovY, t);
    camPose.nearPlane = lerp(outLeft->camPose.nearPlane, outRight->camPose.nearPlane, t);
    camPose.farPlane = lerp(outLeft->camPose.farPlane, outRight->camPose.farPlane, t);

    return camPose;
}


inline CAMERAPOSE Evaluate_StepPose( CINEMATIC_KEYFRAME*& outLeft, CINEMATIC_KEYFRAME*& outRight, float t)
{
    CAMERAPOSE camPose {};

    camPose.position = outLeft->camPose.position;
    camPose.rotation = outLeft->camPose.rotation;
    camPose.fovY = outLeft->camPose.fovY;
    camPose.nearPlane = outLeft->camPose.nearPlane;
    camPose.farPlane = outLeft->camPose.farPlane;
    return camPose;
}


inline CAMERAPOSE Evaluate_EaseInOutPose(CINEMATIC_KEYFRAME*& outLeft, CINEMATIC_KEYFRAME*& outRight, float t)
{
    CAMERAPOSE camPose {};

    float smoothT = Math::GetSmoothStepRatio(t);
    camPose.position = Vec3::SmoothStep(outLeft->camPose.position, outRight->camPose.position, smoothT);
    camPose.rotation = LerpCinematicRotationDegreesShortest(outLeft->camPose.rotation, outRight->camPose.rotation, smoothT);
    camPose.fovY = lerp(outLeft->camPose.fovY, outRight->camPose.fovY, smoothT);
    camPose.nearPlane = lerp(outLeft->camPose.nearPlane, outRight->camPose.nearPlane, smoothT);
    camPose.farPlane = lerp(outLeft->camPose.farPlane, outRight->camPose.farPlane, smoothT);

    return camPose;
}

inline CAMERAPOSE Evaluate_EaseInPose(CINEMATIC_KEYFRAME*& outLeft, CINEMATIC_KEYFRAME*& outRight, float t)
{
    CAMERAPOSE camPose {};

    float easeInT = t * t;
    camPose.position = Vec3::Lerp(outLeft->camPose.position, outRight->camPose.position,easeInT);
    camPose.rotation = LerpCinematicRotationDegreesShortest(outLeft->camPose.rotation, outRight->camPose.rotation, easeInT);
    camPose.fovY = lerp(outLeft->camPose.fovY, outRight->camPose.fovY, easeInT);
    camPose.nearPlane = lerp(outLeft->camPose.nearPlane, outRight->camPose.nearPlane, easeInT);
    camPose.farPlane = lerp(outLeft->camPose.farPlane, outRight->camPose.farPlane, easeInT);

    return camPose;
}

inline CAMERAPOSE Evaluate_EaseOutPose(CINEMATIC_KEYFRAME*& outLeft, CINEMATIC_KEYFRAME*& outRight, float t)
{
    CAMERAPOSE camPose {};

    float easeInT = 1.f - (1.f - t) * (1.f - t);
    camPose.position = Vec3::Lerp(outLeft->camPose.position, outRight->camPose.position,easeInT);
    camPose.rotation = LerpCinematicRotationDegreesShortest(outLeft->camPose.rotation, outRight->camPose.rotation, easeInT);
    camPose.fovY = lerp(outLeft->camPose.fovY, outRight->camPose.fovY, easeInT);
    camPose.nearPlane = lerp(outLeft->camPose.nearPlane, outRight->camPose.nearPlane, easeInT);
    camPose.farPlane = lerp(outLeft->camPose.farPlane, outRight->camPose.farPlane, easeInT);

    return camPose;
}

inline CAMERAPOSE Evaluate_CinematicPose(CINEMATIC_TRACK* cameraTrack, float currentFrameExact, CINEMATIC_KEYFRAME*& outLeft, CINEMATIC_KEYFRAME*& outRight)
{
    Find_SurroundingKeyFrames(cameraTrack, currentFrameExact, outLeft, outRight);

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

    float t = (currentFrameExact - static_cast<float>(outLeft->frame)) / static_cast<float>(outRight->frame - outLeft->frame);

    switch(outLeft->interpolation)
    {
    case InterpolationMode::Linear:
        return  Evaluate_LinearPose(outLeft, outRight, t);

    case InterpolationMode::Step:
        return  Evaluate_StepPose(outLeft, outRight, t);

    case InterpolationMode::EaseInOut:
        return  Evaluate_EaseInOutPose(outLeft, outRight, t);

    case InterpolationMode::EaseIn:
        return Evaluate_EaseInPose(outLeft, outRight, t);

    case InterpolationMode::EaseOut:
        return Evaluate_EaseOutPose(outLeft, outRight,t);

    default:
    {
        // Reuse the same scalar easing table so battle clips and cinematic camera keys stay predictable.
        const float easedT = Evaluate_BattleCameraClipInterpolation(outLeft->interpolation, t);
        CAMERAPOSE easedPose{};
        easedPose.position = Vec3::Lerp(outLeft->camPose.position, outRight->camPose.position, easedT);
        easedPose.rotation = LerpCinematicRotationDegreesShortest(outLeft->camPose.rotation, outRight->camPose.rotation, easedT);
        easedPose.fovY = lerp(outLeft->camPose.fovY, outRight->camPose.fovY, easedT);
        easedPose.nearPlane = lerp(outLeft->camPose.nearPlane, outRight->camPose.nearPlane, easedT);
        easedPose.farPlane = lerp(outLeft->camPose.farPlane, outRight->camPose.farPlane, easedT);
        return easedPose;
    }
    }

    return camPose;
}

inline float Evaluate_CinematicSequenceTime(const CINEMATIC_SEQUENCE& sequence, float currentFrameExact)
{
    const int fps = std::max(sequence.fps, 1);
    const float relativeFrame = std::max(currentFrameExact - static_cast<float>(sequence.startFrame), 0.f);
    return relativeFrame / static_cast<float>(fps);
}

inline Client::CameraModifierSample Evaluate_CinematicModifierSample(const CINEMATIC_SEQUENCE& sequence, float currentFrameExact)
{
    return Sample_CameraModifierClip(sequence.modifierClip, Evaluate_CinematicSequenceTime(sequence, currentFrameExact));
}
