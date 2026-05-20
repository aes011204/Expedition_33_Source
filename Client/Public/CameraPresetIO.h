#pragma once
#include "PreviewShotTable.h"

NS_BEGIN(Client)
class TargetCamera;

class CameraPresetIO
{
public:
    CameraPresetIO();
    virtual ~CameraPresetIO();

public:
    static void SavePresetToFile(TargetCameraMode cameraState, const TargetCameraPreset& preset);
    static bool LoadPresetFromFile(TargetCameraMode cameraState, TargetCameraPreset& outPreset);
    static void SavePreviewToFile(PreviewShotPanel panel, HeroId heroId, const PreviewShotPreset& preset);
    static bool LoadPreviewFromFile(PreviewShotPanel panel, HeroId heroId, PreviewShotPreset& outPreset);
};
NS_END
