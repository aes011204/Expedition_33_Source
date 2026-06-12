#pragma once

enum class InterpolationMode
{
    Step,
    Linear,
    EaseInOut,
    EaseIn,
    EaseOut
};

struct CameraKeyframe
{
    float time = 0.f;
    float value = 0.f;
    InterpolationMode interpolation = InterpolationMode::Linear;
};

struct CameraTrack
{
    vector<CameraKeyframe> keyframes;
};
