#pragma once

#include "BattleCameraClipTypes.h"
#include "Engine_Typedef.h"
#include <algorithm>
#include <cmath>

NS_BEGIN(Client)

inline float Sample_BattleCameraTrack(const BattleCameraTrack& track, float time)
{
    if (track.keyframes.empty())
        return 0.f;

    if (time <= track.keyframes.front().time)
        return track.keyframes.front().value;

    if (time >= track.keyframes.back().time)
        return track.keyframes.back().value;

    for (size_t i = 0; i + 1 < track.keyframes.size(); ++i)
    {
        const BattleCameraKeyframe& lhs = track.keyframes[i];
        const BattleCameraKeyframe& rhs = track.keyframes[i + 1];

        if (time < lhs.time || time > rhs.time)
            continue;

        const float range = std::max(rhs.time - lhs.time, 0.0001f);
        const float ratio = std::clamp((time - lhs.time) / range, 0.f, 1.f);
        return lhs.value + (rhs.value - lhs.value) * ratio;
    }

    return track.keyframes.back().value;
}

inline BattleCameraClipSample Sample_BattleCameraClip(const BattleCameraClip& clip, float time)
{
    const float sampleTime = std::clamp(time, 0.f, std::max(clip.duration, 0.f));

    BattleCameraClipSample sample{};
    sample.distanceOffset = Sample_BattleCameraTrack(clip.distanceOffset, sampleTime);
    sample.fovOffset = Sample_BattleCameraTrack(clip.fovOffset, sampleTime);
    sample.shakeAmplitude = Sample_BattleCameraTrack(clip.shakeAmplitude, sampleTime);
    sample.sideOffset = Sample_BattleCameraTrack(clip.sideOffset, sampleTime);
    sample.pivotOffset = Sample_BattleCameraTrack(clip.pivotOffset, sampleTime);
    sample.yawOffset = Sample_BattleCameraTrack(clip.yawOffset, sampleTime);
    return sample;
}

inline Vec3 Sample_BattleCameraShakeOffset(float amplitude, float time, const Vec3& right, const Vec3& up)
{
    if (amplitude <= 0.f)
        return Vec3::Zero;

    float strengthScale = 0.05f;
    float frequencyX = 32.f;
    float frequencyY = 47.f;
    float decayRate = 8.f;
    float verticalRatio = 0.8f;
    float scaledAmplitude = amplitude * strengthScale;
    float decay = expf(- time * decayRate);

    const float x = sinf(time * frequencyX) * scaledAmplitude;
    const float y = cosf(time * frequencyY) * scaledAmplitude * verticalRatio * decay;
    return right * x + up * y;
}

NS_END
