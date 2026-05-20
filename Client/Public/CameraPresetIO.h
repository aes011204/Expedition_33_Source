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
    static void SaveTargetPresetToFile(TargetCameraMode cameraMode, const TargetCameraPreset& preset);
    static bool LoadTargetPresetFromFile(TargetCameraMode cameraMode, TargetCameraPreset& outPreset);

    static void SaveBattlePresetToFile(BattleCameraMode cameraMode, const BattleCameraPreset& preset);
    static bool LoadBattlePresetFromFile(BattleCameraMode cameraMode, BattleCameraPreset& outPreset);

    static void SavePreviewToFile(PreviewShotPanel panel, HeroId heroId, const PreviewShotPreset& preset);
    static bool LoadPreviewFromFile(PreviewShotPanel panel, HeroId heroId, PreviewShotPreset& outPreset);

};
NS_END
