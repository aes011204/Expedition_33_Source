#pragma once
#include "BattleCameraClipTypes.h"
#include "CameraPresetTypes.h"

struct BattleCameraProfileKey
{
    BattleCameraMode cameraMode;
    string skillId;
    string phaseId;
};

struct BattleCameraProfile
{
    BattleCameraClip   clip;
    BattleCameraPreset preset;
    
};
