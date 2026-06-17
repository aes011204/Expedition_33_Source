#pragma once
#include "PreviewShotTable.h"
#include "BattleCameraClipTypes.h"

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

    static void SaveBattlePresetToFile(BattleCameraMode cameraMode, const BattleCameraPreset& preset, const string& skillId = "");
    static bool LoadBattlePresetFromFile(BattleCameraMode cameraMode, BattleCameraPreset& outPreset, const string& skillId = "");
    static void SaveBattleDriftToFile(BattleCameraMode mode, const BattleCameraDriftProfile& profile, bool isBossVariant = false);
    static bool LoadBattleDriftFromFile(BattleCameraMode mode, BattleCameraDriftProfile& outProfile, bool isBossVariant = false);

    static void SavePreviewToFile(PreviewShotPanel panel, HeroId heroId, const PreviewShotPreset& preset);
    static bool LoadPreviewFromFile(PreviewShotPanel panel, HeroId heroId, PreviewShotPreset& outPreset);

};
NS_END
