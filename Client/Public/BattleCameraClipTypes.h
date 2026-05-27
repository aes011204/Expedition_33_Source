#pragma once
struct BattleCameraKeyframe
{
    float time = 0.f;
    float value = 0.f;
};

struct BattleCameraTrack
{
    vector<BattleCameraKeyframe> keyframes;
};

struct BattleCameraClip
{
    float duration;

    BattleCameraTrack distanceOffset;
    BattleCameraTrack fovOffset;
    BattleCameraTrack yawOffset;
    BattleCameraTrack sideOffset;
    BattleCameraTrack pivotOffset;
    BattleCameraTrack shakeAmplitude;
    BattleCameraTrack orbitYaw;
    BattleCameraTrack orbitRadius;
    BattleCameraTrack orbitHeight;
    BattleCameraTrack orbitLookHeight;
};

struct BattleCameraClipSample
{
    float distanceOffset = 0.f;
    float fovOffset = 0.f;
    float sideOffset = 0.f;
    float pivotOffset = 0.f;
    float yawOffset = 0.f;
    float shakeAmplitude = 0.f;
    float orbitYaw = 0.f;
    float orbitRadius = 0.f;
    float orbitHeight = 0.f;
    float orbitLookHeight = 0.f;
};

