#pragma once

#include "Client_Defines.h"
#include "ScalarTrackTypes.h"

NS_BEGIN(Client)

struct CameraModifierClip
{
    CameraTrack fovOffset;
    CameraTrack shakeAmplitude;
    CameraTrack shakeFrequencyX;
    CameraTrack shakeFrequencyY;
    CameraTrack shakeDecayRateX;
    CameraTrack shakeDecayRate;
    CameraTrack shakeVerticalRatio;
    CameraTrack shakeStrengthScale;
};

struct CameraModifierSample
{
    float fovOffset = 0.f;
    float shakeAmplitude = 0.f;
    float shakeFrequencyX = 32.f;
    float shakeFrequencyY = 47.f;
    float shakeDecayRateX = 0.f;
    float shakeDecayRate = 8.f;
    float shakeVerticalRatio = 0.8f;
    float shakeStrengthScale = 0.05f;
};

NS_END
