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
};

struct  BattleCameraClipSample
{
    float distanceOffset;
    float fovOffset;
    float sideOffset;
    float pivotOffset;
    float yawOffset;
    float shakeAmplitude;
};

