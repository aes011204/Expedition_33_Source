#pragma once

#include "Cinematic_Types.h"

enum class BattleCameraClipTimeMode : uint8
{
    AnimationSync,
    Independent
};

struct BattleCameraKeyframe
{
    float time = 0.f;
    float value = 0.f;
    InterpolationMode interpolation = InterpolationMode::Linear;
};

struct BattleCameraTrack
{
    vector<BattleCameraKeyframe> keyframes;
};

struct BattleCameraClip
{
    float duration = 1.f;
    BattleCameraClipTimeMode timeMode = BattleCameraClipTimeMode::AnimationSync;

    BattleCameraTrack distanceOffset;
    BattleCameraTrack fovOffset;
    BattleCameraTrack yawOffset;
    BattleCameraTrack sideOffset;
    BattleCameraTrack pivotOffset;
    BattleCameraTrack heightOffset;
    BattleCameraTrack aimOffset;
    BattleCameraTrack lookHeightOffset;
    BattleCameraTrack orbitYaw;
    BattleCameraTrack orbitRadius;
    BattleCameraTrack orbitHeight;
    BattleCameraTrack orbitLookHeight;
    BattleCameraTrack shakeAmplitude;
    BattleCameraTrack shakeFrequencyX;
    BattleCameraTrack shakeFrequencyY;
    BattleCameraTrack shakeDecayRateX;
    BattleCameraTrack shakeDecayRate;
    BattleCameraTrack shakeVerticalRatio;
    BattleCameraTrack shakeStrengthScale;
};

struct BattleCameraClipSample
{
    float distanceOffset = 0.f;
    float fovOffset = 0.f;
    float sideOffset = 0.f;
    float pivotOffset = 0.f;
    float heightOffset = 0.f;
    float aimOffset = 0.f;
    float lookHeightOffset = 0.f;
    float yawOffset = 0.f;
    float orbitYaw = 0.f;
    float orbitRadius = 0.f;
    float orbitHeight = 0.f;
    float orbitLookHeight = 0.f;
    float shakeAmplitude = 0.f;
    float shakeFrequencyX = 32.f;
    float shakeFrequencyY = 47.f;
    float shakeDecayRateX = 0.f;
    float shakeDecayRate = 8.f;
    float shakeVerticalRatio = 0.8f;
    float shakeStrengthScale = 0.05f;
};
