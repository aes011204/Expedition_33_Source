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
    static void SaveDialogueCameraClipToFile(const string& presetId, const DialogueCameraClip& clip);
    static bool LoadDialogueCameraClipFromFile(const string& presetId, DialogueCameraClip& clip);
    static void SaveSoulLikeCameraClipToFile(const string& presetKey, const BattleCameraClip& clip);
    static bool LoadSoulLikeCameraClipFromFile(const string& presetKey, BattleCameraClip& clip);
};
NS_END
