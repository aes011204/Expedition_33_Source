#pragma once

#include "ScalarTrackTypes.h"

enum class BattleCameraClipTimeMode : uint8
{
    AnimationSync,
    Independent
};

struct BattleCameraClip
{
    float duration = 1.f;
    BattleCameraClipTimeMode timeMode = BattleCameraClipTimeMode::AnimationSync;

    CameraTrack distanceOffset;
    CameraTrack fovOffset;
    CameraTrack yawOffset;
    CameraTrack sideOffset;
    CameraTrack pivotOffset;
    CameraTrack heightOffset;
    CameraTrack aimOffset;
    CameraTrack lookHeightOffset;
    CameraTrack orbitYaw;
    CameraTrack orbitRadius;
    CameraTrack orbitHeight;
    CameraTrack orbitLookHeight;
    CameraTrack shakeAmplitude;
    CameraTrack shakeFrequencyX;
    CameraTrack shakeFrequencyY;
    CameraTrack shakeDecayRateX;
    CameraTrack shakeDecayRate;
    CameraTrack shakeVerticalRatio;
    CameraTrack shakeStrengthScale;
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
