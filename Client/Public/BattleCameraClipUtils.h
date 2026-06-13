#pragma once

#include "BattleCameraClipTypes.h"
#include "CameraModifierTypes.h"
#include "Engine_Typedef.h"
#include <algorithm>
#include <cmath>

NS_BEGIN(Client)

inline float Evaluate_BattleCameraClipInterpolation(InterpolationMode interpolation, float t)
{
    const float clampedT = std::clamp(t, 0.f, 1.f);
    constexpr float kBackOvershoot = 1.70158f;

    switch (interpolation)
    {
    case InterpolationMode::Step:
        return 0.f;
    case InterpolationMode::Linear:
        return clampedT;
    case InterpolationMode::EaseIn:
        return clampedT * clampedT;
    case InterpolationMode::EaseOut:
    {
        const float invT = 1.f - clampedT;
        return 1.f - invT * invT;
    }
    case InterpolationMode::EaseInStrong:
        return clampedT * clampedT * clampedT;
    case InterpolationMode::EaseOutStrong:
    {
        const float invT = 1.f - clampedT;
        return 1.f - invT * invT * invT;
    }
    case InterpolationMode::EaseInOutStrong:
        return clampedT < 0.5f
            ? 4.f * clampedT * clampedT * clampedT
            : 1.f - powf(-2.f * clampedT + 2.f, 3.f) * 0.5f;
    case InterpolationMode::EaseInCubic:
        return clampedT * clampedT * clampedT;
    case InterpolationMode::EaseOutCubic:
        return 1.f - powf(1.f - clampedT, 3.f);
    case InterpolationMode::EaseInExpo:
        return clampedT <= 0.f ? 0.f : powf(2.f, 10.f * clampedT - 10.f);
    case InterpolationMode::EaseOutExpo:
        return clampedT >= 1.f ? 1.f : 1.f - powf(2.f, -10.f * clampedT);
    case InterpolationMode::EaseOutBack:
    {
        const float x = clampedT - 1.f;
        return 1.f + (kBackOvershoot + 1.f) * x * x * x + kBackOvershoot * x * x;
    }
    case InterpolationMode::EaseInOut:
    default:
        return clampedT < 0.5f
            ? 2.f * clampedT * clampedT
            : 1.f - powf(-2.f * clampedT + 2.f, 2.f) * 0.5f;
    }
}

inline float Sample_BattleCameraTrack(const CameraTrack& track, float time)
{
    if (track.keyframes.empty())
        return 0.f;

    if (time <= track.keyframes.front().time)
        return track.keyframes.front().value;

    if (time >= track.keyframes.back().time)
        return track.keyframes.back().value;

    for (size_t i = 0; i + 1 < track.keyframes.size(); ++i)
    {
        const CameraKeyframe& lhs = track.keyframes[i];
        const CameraKeyframe& rhs = track.keyframes[i + 1];

        if (time < lhs.time || time > rhs.time)
            continue;

        const float range = std::max(rhs.time - lhs.time, 0.0001f);
        const float ratio = std::clamp((time - lhs.time) / range, 0.f, 1.f);
        const float easedRatio = Evaluate_BattleCameraClipInterpolation(lhs.interpolation, ratio);
        return lhs.value + (rhs.value - lhs.value) * easedRatio;
    }

    return track.keyframes.back().value;
}

inline CameraModifierClip Extract_CameraModifierClip(const BattleCameraClip& clip)
{
    CameraModifierClip modifier{};
    modifier.fovOffset = clip.fovOffset;
    modifier.shakeAmplitude = clip.shakeAmplitude;
    modifier.shakeFrequencyX = clip.shakeFrequencyX;
    modifier.shakeFrequencyY = clip.shakeFrequencyY;
    modifier.shakeDecayRateX = clip.shakeDecayRateX;
    modifier.shakeDecayRate = clip.shakeDecayRate;
    modifier.shakeVerticalRatio = clip.shakeVerticalRatio;
    modifier.shakeStrengthScale = clip.shakeStrengthScale;
    return modifier;
}

inline CameraModifierSample Extract_CameraModifierSample(const BattleCameraClipSample& clipSample)
{
    CameraModifierSample modifier{};
    modifier.fovOffset = clipSample.fovOffset;
    modifier.shakeAmplitude = clipSample.shakeAmplitude;
    modifier.shakeFrequencyX = clipSample.shakeFrequencyX;
    modifier.shakeFrequencyY = clipSample.shakeFrequencyY;
    modifier.shakeDecayRateX = clipSample.shakeDecayRateX;
    modifier.shakeDecayRate = clipSample.shakeDecayRate;
    modifier.shakeVerticalRatio = clipSample.shakeVerticalRatio;
    modifier.shakeStrengthScale = clipSample.shakeStrengthScale;
    return modifier;
}

inline CameraModifierSample Sample_CameraModifierClip(const CameraModifierClip& clip, float time)
{
    CameraModifierSample sample{};
    auto sampleTrackOrDefault = [time](const CameraTrack& track, float defaultValue)
    {
        return track.keyframes.empty()
            ? defaultValue
            : Sample_BattleCameraTrack(track, time);
    };

    sample.fovOffset = Sample_BattleCameraTrack(clip.fovOffset, time);
    sample.shakeAmplitude = Sample_BattleCameraTrack(clip.shakeAmplitude, time);
    sample.shakeFrequencyX = sampleTrackOrDefault(clip.shakeFrequencyX, sample.shakeFrequencyX);
    sample.shakeFrequencyY = sampleTrackOrDefault(clip.shakeFrequencyY, sample.shakeFrequencyY);
    sample.shakeDecayRateX = sampleTrackOrDefault(clip.shakeDecayRateX, sample.shakeDecayRateX);
    sample.shakeDecayRate = sampleTrackOrDefault(clip.shakeDecayRate, sample.shakeDecayRate);
    sample.shakeVerticalRatio = sampleTrackOrDefault(clip.shakeVerticalRatio, sample.shakeVerticalRatio);
    sample.shakeStrengthScale = sampleTrackOrDefault(clip.shakeStrengthScale, sample.shakeStrengthScale);
    return sample;
}

inline Vec3 Sample_CameraModifierShakeOffset(
    const CameraModifierSample& modifierSample,
    float time,
    const Vec3& right,
    const Vec3& up)
{
    if (modifierSample.shakeAmplitude <= 0.f)
        return Vec3::Zero;

    const float scaledAmplitude = modifierSample.shakeAmplitude * modifierSample.shakeStrengthScale;
    const float decayX = modifierSample.shakeDecayRateX > 0.f
        ? expf(-time * modifierSample.shakeDecayRateX)
        : 1.f;
    const float decayY = modifierSample.shakeDecayRate > 0.f
        ? expf(-time * modifierSample.shakeDecayRate)
        : 1.f;
    constexpr float kTau = 6.28318530718f;

    const float x = sinf(time * modifierSample.shakeFrequencyX * kTau) * scaledAmplitude * decayX;
    const float y = cosf(time * modifierSample.shakeFrequencyY * kTau) * scaledAmplitude * modifierSample.shakeVerticalRatio * decayY;
    return right * x + up * y;
}

inline BattleCameraClipSample Sample_BattleCameraClip(const BattleCameraClip& clip, float time)
{
    const float sampleTime = std::clamp(time, 0.f, std::max(clip.duration, 0.f));

    BattleCameraClipSample sample{};
    const CameraModifierSample modifierSample =
        Sample_CameraModifierClip(Extract_CameraModifierClip(clip), sampleTime);

    sample.distanceOffset = Sample_BattleCameraTrack(clip.distanceOffset, sampleTime);
    sample.sideOffset = Sample_BattleCameraTrack(clip.sideOffset, sampleTime);
    sample.pivotOffset = Sample_BattleCameraTrack(clip.pivotOffset, sampleTime);
    sample.heightOffset = Sample_BattleCameraTrack(clip.heightOffset, sampleTime);
    sample.aimOffset = Sample_BattleCameraTrack(clip.aimOffset, sampleTime);
    sample.lookHeightOffset = Sample_BattleCameraTrack(clip.lookHeightOffset, sampleTime);
    sample.yawOffset = Sample_BattleCameraTrack(clip.yawOffset, sampleTime);
    sample.orbitYaw = Sample_BattleCameraTrack(clip.orbitYaw, sampleTime);
    sample.orbitRadius = Sample_BattleCameraTrack(clip.orbitRadius, sampleTime);
    sample.orbitHeight = Sample_BattleCameraTrack(clip.orbitHeight, sampleTime);
    sample.orbitLookHeight = Sample_BattleCameraTrack(clip.orbitLookHeight, sampleTime);
    sample.fovOffset = modifierSample.fovOffset;
    sample.shakeAmplitude = modifierSample.shakeAmplitude;
    sample.shakeFrequencyX = modifierSample.shakeFrequencyX;
    sample.shakeFrequencyY = modifierSample.shakeFrequencyY;
    sample.shakeDecayRateX = modifierSample.shakeDecayRateX;
    sample.shakeDecayRate = modifierSample.shakeDecayRate;
    sample.shakeVerticalRatio = modifierSample.shakeVerticalRatio;
    sample.shakeStrengthScale = modifierSample.shakeStrengthScale;

    return sample;
}

inline Vec3 Sample_BattleCameraShakeOffset(float amplitude, float verticalRatio, float strengthScale, float frequencyX, float frequencyY, float decayRateX, float decayRateY, float time, const Vec3& right, const Vec3& up)
{
    CameraModifierSample modifierSample{};
    modifierSample.shakeAmplitude = amplitude;
    modifierSample.shakeVerticalRatio = verticalRatio;
    modifierSample.shakeStrengthScale = strengthScale;
    modifierSample.shakeFrequencyX = frequencyX;
    modifierSample.shakeFrequencyY = frequencyY;
    modifierSample.shakeDecayRateX = decayRateX;
    modifierSample.shakeDecayRate = decayRateY;
    return Sample_CameraModifierShakeOffset(modifierSample, time, right, up);
}

NS_END
