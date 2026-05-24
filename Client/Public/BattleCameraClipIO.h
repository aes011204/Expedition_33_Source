#pragma once
#include "BattleCameraClipTypes.h"
#include "CameraPresetTypes.h"

NS_BEGIN(Client)

class BattleCameraClipIO
{
public:
    BattleCameraClipIO();
    virtual ~BattleCameraClipIO();

public:
    static void SaveBattleCameraClipToFile(BattleCameraMode cameraMode, const BattleCameraClip& clip, const string& skillId = "");
    static bool LoadBattleCameraClipFromFile(BattleCameraMode cameraMode, BattleCameraClip& clip, const string& skillId = "");
};
NS_END
