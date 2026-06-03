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
    auto sampleTrackOrDefault = [sampleTime](const BattleCameraTrack& track, float defaultValue)
    {
        return track.keyframes.empty()
            ? defaultValue
            : Sample_BattleCameraTrack(track, sampleTime);
    };

    sample.distanceOffset = Sample_BattleCameraTrack(clip.distanceOffset, sampleTime);
    sample.fovOffset = Sample_BattleCameraTrack(clip.fovOffset, sampleTime);
    sample.shakeAmplitude = Sample_BattleCameraTrack(clip.shakeAmplitude, sampleTime);
    sample.sideOffset = Sample_BattleCameraTrack(clip.sideOffset, sampleTime);
    sample.pivotOffset = Sample_BattleCameraTrack(clip.pivotOffset, sampleTime);
    sample.yawOffset = Sample_BattleCameraTrack(clip.yawOffset, sampleTime);
    sample.orbitYaw = Sample_BattleCameraTrack(clip.orbitYaw, sampleTime);
    sample.orbitRadius = Sample_BattleCameraTrack(clip.orbitRadius, sampleTime);
    sample.orbitHeight = Sample_BattleCameraTrack(clip.orbitHeight, sampleTime);
    sample.orbitLookHeight = Sample_BattleCameraTrack(clip.orbitLookHeight, sampleTime);
    sample.shakeFrequencyX = sampleTrackOrDefault(clip.shakeFrequencyX, sample.shakeFrequencyX);
    sample.shakeFrequencyY = sampleTrackOrDefault(clip.shakeFrequencyY, sample.shakeFrequencyY);
    sample.shakeDecayRateX = sampleTrackOrDefault(clip.shakeDecayRateX, sample.shakeDecayRateX);
    sample.shakeDecayRate = sampleTrackOrDefault(clip.shakeDecayRate, sample.shakeDecayRate);
    sample.shakeVerticalRatio = sampleTrackOrDefault(clip.shakeVerticalRatio, sample.shakeVerticalRatio);
    sample.shakeStrengthScale = sampleTrackOrDefault(clip.shakeStrengthScale, sample.shakeStrengthScale);

    return sample;
}

inline Vec3 Sample_BattleCameraShakeOffset(float amplitude, float verticalRatio, float strengthScale, float frequencyX, float frequencyY, float decayRateX, float decayRateY, float time, const Vec3& right, const Vec3& up)
{
    if (amplitude <= 0.f)
        return Vec3::Zero;

    float scaledAmplitude = amplitude * strengthScale;
    float decayX = decayRateX > 0.f ? expf(- time * decayRateX) : 1.f;
    float decayY = decayRateY > 0.f ? expf(- time * decayRateY) : 1.f;

    const float x = sinf(time * frequencyX) * scaledAmplitude * decayX;
    const float y = cosf(time * frequencyY) * scaledAmplitude * verticalRatio * decayY;
    return right * x + up * y;
}

NS_END
